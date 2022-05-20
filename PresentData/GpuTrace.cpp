// Copyright (C) 2021-2022 Intel Corporation
// SPDX-License-Identifier: MIT

#include "PresentMonTraceConsumer.hpp"

namespace {

#if DEBUG_VERBOSE

// Call whenever a Dma starts on an idle engine 
void DebugFirstDmaStart()
{
    printf("                             FirstDmaTime\n");
}

// Call whenever Dma time is added to a queue
void DebugDmaAccumulated(uint64_t currentTime, uint64_t addedTime)
{
    printf("                             DmaTimeAccumulated: ");
    DebugPrintTimeDelta(currentTime);
    printf(" + ");
    DebugPrintTimeDelta(addedTime);
    printf(" => ");
    DebugPrintTimeDelta(currentTime + addedTime);
    printf("\n");
}

#else
#define DebugFirstDmaStart()
#define DebugDmaAccumulated(currentTime, addedTime) (void) currentTime, addedTime
#endif

}

GpuTrace::GpuTrace(PMTraceConsumer* pmConsumer)
    : mPMConsumer(pmConsumer)
    , mCloudStreamingProcessId(0)
{
}

void GpuTrace::RegisterDevice(uint64_t hDevice, uint64_t pDxgAdapter)
{
    // Sometimes there are duplicate start events
    DebugAssert(mDevices.find(hDevice) == mDevices.end() || mDevices.find(hDevice)->second == pDxgAdapter);

    mDevices.emplace(hDevice, pDxgAdapter);
}

void GpuTrace::UnregisterDevice(uint64_t hDevice)
{
    // Sometimes there are duplicate stop events so it's ok if it's already removed
    mDevices.erase(hDevice);
}

void GpuTrace::RegisterContext(uint64_t hContext, uint64_t hDevice, uint32_t nodeOrdinal, uint32_t processId)
{
    auto deviceIter = mDevices.find(hDevice);
    if (deviceIter == mDevices.end()) {
        DebugAssert(false);
        return;
    }
    auto pDxgAdapter = deviceIter->second;
    auto node = &mNodes[pDxgAdapter].emplace(nodeOrdinal, Node{}).first->second;

    // Sometimes there are duplicate start events, make sure that they say the same thing
    DebugAssert(mContexts.find(hDevice) == mContexts.end() || mContexts.find(hDevice)->second.mNode == node);

    auto context = &mContexts.emplace(hContext, Context()).first->second;
    context->mPacketTrace = nullptr;
    context->mNode = node;
    context->mIsVideoEncoderForCloudStreamingApp = false;

    if (processId != 0) {
        SetContextProcessId(context, processId);
    }
}

void GpuTrace::UnregisterContext(uint64_t hContext)
{
    // Sometimes there are duplicate stop events so it's ok if it's already removed
    mContexts.erase(hContext);
}

void GpuTrace::SetEngineType(uint64_t pDxgAdapter, uint32_t nodeOrdinal, Microsoft_Windows_DxgKrnl::DXGK_ENGINE engineType)
{
    // Node should already be created (DxgKrnl::Context_Start comes
    // first) but just to be sure...
    auto node = &mNodes[pDxgAdapter].emplace(nodeOrdinal, Node{}).first->second;

    if (engineType == Microsoft_Windows_DxgKrnl::DXGK_ENGINE::VIDEO_DECODE ||
        engineType == Microsoft_Windows_DxgKrnl::DXGK_ENGINE::VIDEO_ENCODE ||
        engineType == Microsoft_Windows_DxgKrnl::DXGK_ENGINE::VIDEO_PROCESSING) {
        node->mIsVideo = true;
    }

    if (engineType == Microsoft_Windows_DxgKrnl::DXGK_ENGINE::VIDEO_DECODE) {
        node->mIsVideoDecode = true;
    }
}

void GpuTrace::SetContextProcessId(Context* context, uint32_t processId)
{
    auto p = mProcessFrameInfo.emplace(processId, ProcessFrameInfo{});

    if (!mPMConsumer->mTrackGPUVideo) {
        context->mPacketTrace = &p.first->second.mOtherEngines;
        return;
    }

    context->mPacketTrace = context->mNode->mIsVideo || context->mNode->mIsVideoDecode
        ? &p.first->second.mVideoEngines
        : &p.first->second.mOtherEngines;

    if (p.second) {
        if (mCloudStreamingProcessId == 0) {
            std::string processName;

            if (mPMConsumer->mNTProcessNames.empty()) { // using as proxy for runtime collection, should we have a variable for this?
                auto handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
                if (handle != nullptr) {
                    char path[MAX_PATH] = {};
                    DWORD numChars = sizeof(path);
                    if (QueryFullProcessImageNameA(handle, 0, path, &numChars)) {
                        for (; numChars > 0; --numChars) {
                            if (path[numChars - 1] == '/' ||
                                path[numChars - 1] == '\\') {
                                break;
                            }
                        }
                        processName = path + numChars;
                    }
                    CloseHandle(handle);
                }
            } else {
                auto ii = mPMConsumer->mNTProcessNames.find(processId);
                if (ii != mPMConsumer->mNTProcessNames.end()) {
                    processName = ii->second;
                }
            }

            if (!processName.empty() && (_stricmp(processName.c_str(), "parsecd.exe") == 0 ||
                                         _stricmp(processName.c_str(), "intel-cloud-screen-capture.exe") == 0 ||
                                         _stricmp(processName.c_str(), "nvEncDXGIOutputDuplicationSample.exe") == 0)) {
                mCloudStreamingProcessId = processId;
            }
        }
    }

    if (context->mNode->mIsVideoDecode && processId == mCloudStreamingProcessId) {
        context->mIsVideoEncoderForCloudStreamingApp = true;
    }
}

void GpuTrace::EnqueueDmaPacket(uint64_t hContext, uint32_t sequenceId, uint64_t timestamp)
{
    // Lookup the context to figure out which node it's running on; this can
    // fail sometimes e.g. if parsing the beginning of an ETL file where we can
    // get packet events before the context mapping.
    auto ii = mContexts.find(hContext);
    if (ii == mContexts.end()) {
        return;
    }

    auto context = &ii->second;
    auto packetTrace = context->mPacketTrace;
    auto node = context->mNode;

    // A very rare (never observed) race exists where packetTrace can still be
    // nullptr here.  The context must have been created and this packet must
    // have been submitted to the queue before the capture started.
    //
    // In this case, we have to ignore the DMA packet otherwise the node and
    // process tracking will become out of sync.
    if (packetTrace == nullptr) {
        return;
    }

    if (node->mQueueCount == Node::MAX_QUEUE_SIZE) {
        // mPacketTrace/mSequenceId arrays are too small (or, DmaPacket_Info
        // events didn't fire for some reason).  This seems to always hit when
        // an application closes...
        return;
    }

    // Enqueue the packet
    auto queueIndex = (node->mQueueIndex + node->mQueueCount) % Node::MAX_QUEUE_SIZE;
    node->mPacketTrace[queueIndex] = packetTrace;
    node->mSequenceId[queueIndex] = sequenceId;
    node->mQueueCount += 1;

    // If the queue was empty, the packet starts running right away, otherwise
    // it is just enqueued and will start running after all previous packets
    // complete.
    if (node->mQueueCount == 1) {
        packetTrace->mRunningPacketCount += 1;
        if (packetTrace->mRunningPacketCount == 1) {
            packetTrace->mRunningPacketStartTime = timestamp;
            if (packetTrace->mFirstPacketTime == 0) {
                packetTrace->mFirstPacketTime = timestamp;
                DebugFirstDmaStart();
            }
        }
    }
}

uint32_t GpuTrace::CompleteDmaPacket(uint64_t hContext, uint32_t sequenceId, uint64_t timestamp)
{
    // Lookup the context to figure out which node it's running on; this can
    // fail sometimes e.g. if parsing the beginning of an ETL file where we can
    // get packet events before the context mapping.
    auto ii = mContexts.find(hContext);
    if (ii == mContexts.end()) {
        return 0;
    }

    auto context = &ii->second;
    auto packetTrace = context->mPacketTrace;
    auto node = context->mNode;

    // It's possible to miss DmaPacket events during realtime analysis, so try
    // to handle it gracefully here.
    //
    // If we get a DmaPacket_Info event for a packet that we didn't get a
    // DmaPacket_Start event for (or that we ignored because we didn't know the
    // process yet) then sequenceId will be smaller than expected.  If this
    // happens, we ignore the DmaPacket_Info event which means that, if there
    // was idle time before the missing DmaPacket_Start event,
    // mAccumulatedPacketTime will be too large.
    //
    // measured: ----------------  -------     ---------------------
    //                                            [---   [--
    // actual:   [-----]  [-----]  [-----]     [-----]-----]-------]
    //           ^     ^  x     ^  ^     ^        x  ^   ^
    //           s1    i1 s2    i2 s3    i3       s2 i1  s3
    auto runningSequenceId = node->mSequenceId[node->mQueueIndex];
    if (packetTrace == nullptr || node->mQueueCount == 0 || sequenceId < runningSequenceId) {
        return 0;
    }

    // If we get a DmaPacket_Start event with no corresponding DmaPacket_Info,
    // then sequenceId will be larger than expected.  If this happens, we seach
    // through the queue for a match and if no match was found then we ignore
    // this event (we missed both the DmaPacket_Start and DmaPacket_Info for
    // the packet).  In this case, both the missing packet's execution time as
    // well as any idle time afterwards will be associated with the previous
    // packet.
    //
    // If a match is found, then we don't know when the pre-match packets ended
    // (nor when the matched packet started).  We treat this case as if the
    // first packet with a missed DmaPacket_Info ran the whole time, and all
    // other packets up to the match executed with zero time.  Any idle time
    // during this range is ignored, and the correct association of gpu work to
    // process will not be correct (unless all these contexts come from the
    // same process).
    //
    // measured: -------  ----------------     ---------------------
    //                                            [---   [--
    // actual:   [-----]  [-----]  [-----]     [-----]-----]-------]
    //           ^     ^  ^     x  ^     ^        ^  ^     x
    //           s1    i1 s2    i2 s3    i3       s2 i1    i2
    if (sequenceId > runningSequenceId) {
        for (uint32_t missingCount = 1; ; ++missingCount) {
            if (missingCount == node->mQueueCount) {
                return 0;
            }

            uint32_t queueIndex = (node->mQueueIndex + missingCount) % Node::MAX_QUEUE_SIZE;
            if (node->mSequenceId[queueIndex] == sequenceId) {
                // Move current packet into this slot
                node->mPacketTrace[queueIndex] = node->mPacketTrace[node->mQueueIndex];
                node->mSequenceId[queueIndex] = node->mSequenceId[node->mQueueIndex];
                node->mQueueIndex = queueIndex;
                node->mQueueCount -= missingCount;

                packetTrace = node->mPacketTrace[node->mQueueIndex];
                break;
            }
        }
    }

    // Pop the completed packet from the queue
    node->mQueueCount -= 1;
    packetTrace->mRunningPacketCount -= 1;

    // If this was the process' last executing packet, accumulate the execution
    // duration into the process' count.
    DebugAssert(packetTrace == node->mPacketTrace[node->mQueueIndex]);
    if (packetTrace->mRunningPacketCount == 0) {
        auto accumulatedTime = timestamp - packetTrace->mRunningPacketStartTime;

        DebugDmaAccumulated(packetTrace->mAccumulatedPacketTime, accumulatedTime);

        packetTrace->mLastPacketTime = timestamp;
        packetTrace->mAccumulatedPacketTime += accumulatedTime;
        packetTrace->mRunningPacketStartTime = 0;
    }

    // If there was another queued packet, start it
    if (node->mQueueCount > 0) {
        node->mQueueIndex = (node->mQueueIndex + 1) % Node::MAX_QUEUE_SIZE;
        packetTrace = node->mPacketTrace[node->mQueueIndex];
        packetTrace->mRunningPacketCount += 1;
        if (packetTrace->mRunningPacketCount == 1) {
            packetTrace->mRunningPacketStartTime = timestamp;
            if (packetTrace->mFirstPacketTime == 0) {
                packetTrace->mFirstPacketTime = timestamp;
                DebugFirstDmaStart();
            }
        }
    }

    // Return the non-zero cloud streaming process id if this is the end of an
    // identified video encode packet on a context used for cloud streaming.
    return context->mIsVideoEncoderForCloudStreamingApp
        ? mCloudStreamingProcessId
        : 0;
}

void GpuTrace::EnqueueQueuePacket(uint32_t processId, uint64_t hContext)
{
    // Create a PacketTrace for this context (for cases where the context was
    // created before the capture was started)
    //
    // mContexts should be empty if mTrackGPU==false.
    auto contextIter = mContexts.find(hContext);
    if (contextIter != mContexts.end()) {
        auto context = &contextIter->second;
        SetContextProcessId(context, processId);
    }
}

void GpuTrace::SetINTCProducerPresentTime(uint32_t processId, uint64_t timestamp)
{
    auto frameInfo = &mProcessFrameInfo.emplace(processId, ProcessFrameInfo{}).first->second;
    frameInfo->mINTCProducerPresentTime = timestamp;
}

void GpuTrace::SetINTCConsumerPresentTime(uint32_t processId, uint64_t timestamp)
{
    auto frameInfo = &mProcessFrameInfo.emplace(processId, ProcessFrameInfo{}).first->second;
    frameInfo->mINTCConsumerPresentTime = timestamp;
}

void GpuTrace::StartINTCTimer(INTCTimer timer, uint32_t processId, uint64_t timestamp)
{
    auto frameInfo = &mProcessFrameInfo.emplace(processId, ProcessFrameInfo{}).first->second;
    auto timerInfo = &frameInfo->mINTCTimers[timer];

    timerInfo->mStartCount += 1;
    if (timerInfo->mStartCount == 1) {
        timerInfo->mStartTime = timestamp;
    }
}

void GpuTrace::StopINTCTimer(INTCTimer timer, uint32_t processId, uint64_t timestamp)
{
    auto frameInfo = &mProcessFrameInfo.emplace(processId, ProcessFrameInfo{}).first->second;
    auto timerInfo = &frameInfo->mINTCTimers[timer];

    if (timerInfo->mStartCount >= 1) {
        timerInfo->mStartCount -= 1;
        if (timerInfo->mStartCount == 0) {
            timerInfo->mAccumulatedTime += timestamp - timerInfo->mStartTime;
            timerInfo->mStartTime = 0;
        }
    }
}

void GpuTrace::StartMakeResident(uint32_t processId, uint64_t timestamp)
{
    auto frameInfo = &mProcessFrameInfo.emplace(processId, ProcessFrameInfo{}).first->second;
    auto timer = &frameInfo->mResidencyTimers[DXGK_RESIDENCY_EVENT_MAKE_RESIDENT];
    timer->mStartTime = timestamp;
}

void GpuTrace::StopMakeResident(uint32_t processId, uint64_t timestamp)
{
    auto frameInfo = &mProcessFrameInfo.emplace(processId, ProcessFrameInfo{}).first->second;
    auto timer = &frameInfo->mResidencyTimers[DXGK_RESIDENCY_EVENT_MAKE_RESIDENT];
    if (timer->mStartTime > 0) {
        timer->mAccumulatedTime += timestamp - timer->mStartTime;
        timer->mStartTime = 0;
    }
}

void GpuTrace::RegisterPagingQueuePacket(uint32_t processId, uint64_t sequenceId)
{
    mPagingSequenceIds[sequenceId] = processId;
}

void GpuTrace::StartPagingQueuePacket(uint64_t sequenceId, uint64_t timestamp)
{
    auto pagingIter = mPagingSequenceIds.find(sequenceId);
    if (pagingIter != mPagingSequenceIds.end()) {
        auto frameInfo = &mProcessFrameInfo.emplace(pagingIter->second, ProcessFrameInfo{}).first->second;
        auto timer = &frameInfo->mResidencyTimers[DXGK_RESIDENCY_EVENT_PAGING_QUEUE_PACKET];

        // If this fires, then either a PagingQueuePacket_Stop event
        // was missed (which is expected but should be rare) or
        // multiple paging packets can execute in parallel (which
        // violates the assumption we're making).  If the latter, this
        // needs to be fixed.
        DebugAssert(timer->mStartTime == 0);

        timer->mStartTime = timestamp;
    }
}

void GpuTrace::StopPagingQueuePacket(uint64_t sequenceId, uint64_t timestamp)
{
    auto pagingIter = mPagingSequenceIds.find(sequenceId);
    if (pagingIter != mPagingSequenceIds.end()) {
        auto frameInfo = &mProcessFrameInfo.emplace(pagingIter->second, ProcessFrameInfo{}).first->second;
        auto timer = &frameInfo->mResidencyTimers[DXGK_RESIDENCY_EVENT_PAGING_QUEUE_PACKET];

        if (timer->mStartTime > 0) {
            timer->mAccumulatedTime += timestamp - timer->mStartTime;
            timer->mStartTime = 0;
        }

        // Stop tracking this sequenceId
        //
        // TODO: If we miss a PagingQueuePacket_Stop, then this will stay
        // in the map.  When we get a PagingQueuePacket_Stop, we should
        // probably also remove any stored sequence id <= sequenceId.
        mPagingSequenceIds.erase(pagingIter);
    }
}

void GpuTrace::CompleteFrame(PresentEvent* pEvent, uint64_t timestamp)
{
    // Note: there is a potential race here because QueuePacket_Stop occurs
    // sometime after DmaPacket_Info it's possible that some small portion of
    // the next frame's GPU work has both started and completed before
    // QueuePacket_Stop and will be attributed to this frame.  However, this is
    // necessarily a small amount of work, and we can't use DMA packets as not
    // all present types create them.
    auto ii = mProcessFrameInfo.find(pEvent->ProcessId);
    if (ii != mProcessFrameInfo.end()) {
        auto frameInfo = &ii->second;
        auto packetTrace = &frameInfo->mOtherEngines;
        auto videoTrace = &frameInfo->mVideoEngines;

        DebugModifyPresent(pEvent);

        // Update GPUStartTime/ReadyTime/GPUDuration if any DMA packets were
        // observed.
        if (packetTrace->mFirstPacketTime != 0) {
            pEvent->GPUStartTime = packetTrace->mFirstPacketTime;
            pEvent->ReadyTime = packetTrace->mLastPacketTime;
            pEvent->GPUDuration = packetTrace->mAccumulatedPacketTime;
            packetTrace->mFirstPacketTime = 0;
            packetTrace->mLastPacketTime = 0;
            packetTrace->mAccumulatedPacketTime = 0;
        }

        pEvent->GPUVideoDuration = videoTrace->mAccumulatedPacketTime;
        videoTrace->mFirstPacketTime = 0;
        videoTrace->mLastPacketTime = 0;
        videoTrace->mAccumulatedPacketTime = 0;

        if (mPMConsumer->mTrackINTCTimers || mPMConsumer->mTrackINTCCpuGpuSync) {
            pEvent->INTC_ProducerPresentTime = frameInfo->mINTCProducerPresentTime;
            pEvent->INTC_ConsumerPresentTime = frameInfo->mINTCConsumerPresentTime;
            frameInfo->mINTCProducerPresentTime = 0;
            frameInfo->mINTCConsumerPresentTime = 0;

            for (uint32_t i = 0; i < INTC_TIMER_COUNT; ++i) {
                if (frameInfo->mINTCTimers[i].mStartTime != 0) {
                    frameInfo->mINTCTimers[i].mAccumulatedTime += timestamp - frameInfo->mINTCTimers[i].mStartTime;
                    frameInfo->mINTCTimers[i].mStartTime = timestamp;
                }
                pEvent->INTC_Timers[i] = frameInfo->mINTCTimers[i].mAccumulatedTime;
                frameInfo->mINTCTimers[i].mAccumulatedTime = 0;
            }
        }

        if (mPMConsumer->mTrackMemoryResidency) {
            for (uint32_t i = 0; i < DXGK_RESIDENCY_EVENT_COUNT; i++)  {
                pEvent->MemoryResidency[i] = frameInfo->mResidencyTimers[i].mAccumulatedTime;
                frameInfo->mResidencyTimers[i].mAccumulatedTime = 0;
            }
        }

        // There are some cases where the QueuePacket_Stop timestamp is before
        // the previous dma packet completes.  e.g., this seems to be typical
        // of DWM present packets.  In these cases, instead of loosing track of
        // the previous dma work, we split it at this time and assign portions
        // to both frames.  Note this is incorrect, as the dma's full cost
        // should be fully attributed to the previous frame.
        if (packetTrace->mRunningPacketCount > 0) {
            auto accumulatedTime = timestamp - packetTrace->mRunningPacketStartTime;

            DebugDmaAccumulated(pEvent->GPUDuration, accumulatedTime);

            pEvent->ReadyTime = timestamp;
            pEvent->GPUDuration += accumulatedTime;
            packetTrace->mFirstPacketTime = timestamp;
            packetTrace->mRunningPacketStartTime = timestamp;

            DebugFirstDmaStart();
        }
        if (videoTrace->mRunningPacketCount > 0) {
            pEvent->GPUVideoDuration += timestamp - videoTrace->mRunningPacketStartTime;
            videoTrace->mFirstPacketTime = timestamp;
            videoTrace->mRunningPacketStartTime = timestamp;
        }
    }
}
