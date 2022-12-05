// Copyright (C) 2021-2022 Intel Corporation
// SPDX-License-Identifier: MIT

#include "PresentMonTraceConsumer.hpp"

namespace {

void DebugPrintAccumulatedGpuTime(uint32_t processId, uint64_t accumulatedTime, uint64_t startTime, uint64_t endTime)
{
    auto addedTime = endTime - startTime;

    if (addedTime > 0ull) {
        printf("                             Accumulated GPU time ProcessId=%u: ", processId);
        PrintTimeDelta(accumulatedTime);
        printf(" + [");
        PrintTime(startTime);
        printf(", ");
        PrintTime(endTime);
        printf("] = ");
        PrintTimeDelta(accumulatedTime + addedTime);
        printf("\n");
    }
}

}

uint32_t GpuTrace::LookupPacketTraceProcessId(PacketTrace* packetTrace) const
{
    for (auto const& pr : mProcessFrameInfo) {
        if (packetTrace == &pr.second.mVideoEngines ||
            packetTrace == &pr.second.mOtherEngines) {
            return pr.first;
        }
    }
    return 0;
}

void GpuTrace::PrintRunningContexts() const
{
    for (auto const& pr : mContexts) {
        auto hContext = pr.first;
        auto const& context = pr.second;
        auto const& node = *context.mNode;

        if (node.mQueueCount > 0) {
            printf("                             hContext=0x%llx [", hContext);

            for (uint32_t i = 0; i < node.mQueueCount; ++i) {
                auto queueIdx = (node.mQueueIndex + i) % Node::MAX_QUEUE_SIZE;

                if (i > 0) {
                    printf("\n                                                          ");
                }

                printf(" SequenceId=%u", node.mSequenceId[queueIdx]);
                if (node.mPacketTrace[queueIdx] == nullptr) {
                    printf(" WAIT");
                } else {
                    printf(" ProcessId=%u", LookupPacketTraceProcessId(node.mPacketTrace[queueIdx]));
                }
            }

            printf(" ]\n");
        }
    }
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
    DebugAssert(mContexts.find(hContext) == mContexts.end() || mContexts.find(hContext)->second.mNode == node);

    auto context = &mContexts.emplace(hContext, Context()).first->second;
    context->mPacketTrace = nullptr;
    context->mNode = node;
    context->mParentContext = 0;
    context->mIsParentContext = false;
    context->mIsHwQueue = false;
    context->mIsVideoEncoderForCloudStreamingApp = false;

    if (processId != 0) {
        SetContextProcessId(context, processId);
    }
}

// HwQueue's are similar to Contexts, but are used with HWS nodes and use the
// following pattern:
//     Context_Start hContext=C hDevice=D NodeOrdinal=N
//     HwQueue_Start hContext=C hHwQueue=0x0 ParentDxgHwQueue=Q1
//     HwQueue_Start hContext=C hHwQueue=H1  ParentDxgHwQueue=Q2
//     HwQueue_Start hContext=C hHwQueue=H2  ParentDxgHwQueue=Q3
//     ...
//     DxgKrnl_QueuePacket_Start hContext=Q2 SubmitSequence=S ...
//     ...
//     DxgKrnl_QueuePacket_Stop hContext=Q2 SubmitSequence=S
//     ...
//     HwQueue_Stop hContext=C hHwQueue=0x0 ParentDxgHwQueue=Q3
//     HwQueue_Stop hContext=C hHwQueue=0x0 ParentDxgHwQueue=Q2
//     HwQueue_Stop hContext=C hHwQueue=0x0 ParentDxgHwQueue=Q1
//     Context_Stop hContext=C
void GpuTrace::RegisterHwQueueContext(uint64_t hContext, uint64_t parentDxgHwQueue)
{
    DebugAssert(mContexts.find(hContext)         != mContexts.end());
    DebugAssert(mContexts.find(parentDxgHwQueue) == mContexts.end());

    // Look up the context C, which we're calling the parent context.  We
    // should already have seen a Context_Start event.
    auto ii = mContexts.find(hContext);
    if (ii == mContexts.end()) {
        return;
    }
    auto parentContext = &ii->second;

    DebugAssert(parentContext->mParentContext == 0);
    DebugAssert(parentContext->mIsHwQueue == false);
    parentContext->mIsParentContext = true;

    // Create a new context for the HWQueue.  Even though they map the same
    // device engine, HWQueues need their own context so that tracked sequence
    // IDs are ordered.
    //
    // If there are two HwQueues that map to the same engine, it's not clear
    // whether or not queue packets running at the same time are really running
    // simultaneous, but since we're counting any duration where at least one
    // node is running it doesn't matter.

    Node* node = new Node();
    node->mQueueIndex = 0;
    node->mQueueCount = 0;
    node->mIsVideo = parentContext->mNode->mIsVideo;
    node->mIsVideoDecode = parentContext->mNode->mIsVideoDecode;

    auto hwQueueContext = &mContexts.emplace(parentDxgHwQueue, Context()).first->second;
    hwQueueContext->mPacketTrace = parentContext->mPacketTrace;
    hwQueueContext->mNode = node;
    hwQueueContext->mParentContext = hContext;
    hwQueueContext->mIsParentContext = false;
    hwQueueContext->mIsHwQueue = true;
    hwQueueContext->mIsVideoEncoderForCloudStreamingApp = false;
}

void GpuTrace::UnregisterContext(uint64_t hContext)
{
    // Sometimes there are duplicate stop events so it's ok if it's already
    // removed
    auto ii = mContexts.find(hContext);
    auto ie = mContexts.end();
    if (ii != ie) {
        auto isParentContext = ii->second.mIsParentContext;
        mContexts.erase(ii);

        if (isParentContext) {
            for (ii = mContexts.begin(); ii != ie; ) {
                auto const& context = ii->second;
                if (context.mParentContext == hContext) {
                    DebugAssert(context.mIsHwQueue);
                    delete context.mNode;
                    ii = mContexts.erase(ii);
                } else {
                    ++ii;
                }
            }
        }
    }
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

    if (mPMConsumer->mTrackGPUVideo && (context->mNode->mIsVideo || context->mNode->mIsVideoDecode)) {
        context->mPacketTrace = &p.first->second.mVideoEngines;
    } else {
        context->mPacketTrace = &p.first->second.mOtherEngines;
    }

    if (mPMConsumer->mTrackGPUVideo) {
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
}

void GpuTrace::StartPacket(PacketTrace* packetTrace, uint64_t timestamp) const
{
    packetTrace->mRunningPacketCount += 1;
    if (packetTrace->mRunningPacketCount == 1) {
        packetTrace->mRunningPacketStartTime = timestamp;
        if (packetTrace->mFirstPacketTime == 0) {
            packetTrace->mFirstPacketTime = timestamp;

            if (IsVerboseTraceEnabled()) {
                printf("                             GPU: pid=%u frame's first work\n", LookupPacketTraceProcessId(packetTrace));
            }
        }
    }
}

void GpuTrace::CompletePacket(PacketTrace* packetTrace, uint64_t timestamp) const
{
    if (IsVerboseTraceEnabled()) {
        DebugPrintAccumulatedGpuTime(LookupPacketTraceProcessId(packetTrace),
                                     packetTrace->mAccumulatedPacketTime,
                                     packetTrace->mRunningPacketStartTime,
                                     timestamp);
    }

    auto accumulatedTime = timestamp - packetTrace->mRunningPacketStartTime;

    packetTrace->mLastPacketTime = timestamp;
    packetTrace->mAccumulatedPacketTime += accumulatedTime;
    packetTrace->mRunningPacketStartTime = 0;
}

void GpuTrace::EnqueueWork(Context* context, uint32_t sequenceId, uint64_t timestamp, bool isWaitPacket)
{
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

    // Enqueue the packet.
    //
    // Wait packets aren't counted as GPU work, but we still need to enqueue
    // them so they block future work.  We encode wait packets by setting their
    // packetTrace to null.  This saves some memory as we don't need the
    // packetTrace pointer for wait packets.
    if (isWaitPacket) {
        packetTrace = nullptr;
    }

    auto queueIndex = (node->mQueueIndex + node->mQueueCount) % Node::MAX_QUEUE_SIZE;
    node->mPacketTrace[queueIndex] = packetTrace;
    node->mSequenceId[queueIndex] = sequenceId;
    node->mQueueCount += 1;

    // If the queue was empty, the packet starts running right away, otherwise
    // it is just enqueued and will start running after all previous packets
    // complete.
    if (packetTrace != nullptr && node->mQueueCount == 1) {
        StartPacket(packetTrace, timestamp);
    }

    if (IsVerboseTraceEnabled()) {
        PrintRunningContexts();
    }
}

bool GpuTrace::CompleteWork(Context* context, uint32_t sequenceId, uint64_t timestamp)
{
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
        return false;
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
                return false;
            }

            uint32_t queueIndex = (node->mQueueIndex + missingCount) % Node::MAX_QUEUE_SIZE;
            if (node->mSequenceId[queueIndex] == sequenceId) {
                // Move current packet into this slot
                node->mPacketTrace[queueIndex] = node->mPacketTrace[node->mQueueIndex];
                node->mSequenceId[queueIndex] = node->mSequenceId[node->mQueueIndex];
                node->mQueueIndex = queueIndex;
                node->mQueueCount -= missingCount;
                break;
            }
        }
    }

    // Pop the completed packet from the queue.
    //
    // If this was the process' last executing packet, accumulate the execution
    // duration into the process' count.
    node->mQueueCount -= 1;

    packetTrace = node->mPacketTrace[node->mQueueIndex];
    if (packetTrace != nullptr) {
        packetTrace->mRunningPacketCount -= 1;
        if (packetTrace->mRunningPacketCount == 0) {
            CompletePacket(packetTrace, timestamp);
        }
    }

    // If there was another queued packet, start it
    if (node->mQueueCount > 0) {
        node->mQueueIndex = (node->mQueueIndex + 1) % Node::MAX_QUEUE_SIZE;

        packetTrace = node->mPacketTrace[node->mQueueIndex];
        if (packetTrace != nullptr) {
            StartPacket(packetTrace, timestamp);
        }
    }

    return true;
}

void GpuTrace::EnqueueQueuePacket(uint64_t hContext, uint32_t sequenceId, uint32_t processId, uint64_t timestamp, bool isWaitPacket)
{
    auto contextIter = mContexts.find(hContext);
    if (contextIter != mContexts.end()) {
        auto context = &contextIter->second;

        // Ensure that the process id is registered with this context, for
        // cases where the context was created before the capture was started
        // so we didn't see a Context_Start event.
        if (context->mPacketTrace == nullptr) {
            SetContextProcessId(context, processId);
        }

        // Use queue packet duration as a proxy for dma duration for cases we
        // don't get dma events for (HWS).
        if (context->mIsHwQueue) {
            EnqueueWork(context, sequenceId, timestamp, isWaitPacket);
        }
    }
}

void GpuTrace::CompleteQueuePacket(uint64_t hContext, uint32_t sequenceId, uint64_t timestamp)
{
    auto contextIter = mContexts.find(hContext);
    if (contextIter != mContexts.end()) {
        auto context = &contextIter->second;

        // Use queue packet duration as a proxy for dma duration for cases we
        // don't get dma events for (HWS).
        if (context->mIsHwQueue) {
            auto trackedWorkWasCompleted = CompleteWork(context, sequenceId, timestamp);

            #pragma warning(suppress: 4127) // conditional expression is constant in release build
            if (IsVerboseTraceEnabled() && trackedWorkWasCompleted) {
                PrintRunningContexts();
            }
        }
    }
}

void GpuTrace::EnqueueDmaPacket(uint64_t hContext, uint32_t sequenceId, uint64_t timestamp)
{
    // Lookup the context.  This can fail sometimes e.g. if parsing the
    // beginning of an ETL file where we can get packet events before the
    // context mapping.
    auto ii = mContexts.find(hContext);
    if (ii == mContexts.end()) {
        return;
    }
    auto context = &ii->second;

    // Should not see any dma packets on a HwQueue
    DebugAssert(!context->mIsHwQueue);

    // Start tracking the work
    bool isWaitPacket = false;
    EnqueueWork(context, sequenceId, timestamp, isWaitPacket);
}

uint32_t GpuTrace::CompleteDmaPacket(uint64_t hContext, uint32_t sequenceId, uint64_t timestamp)
{
    // Lookup the context.  This can fail sometimes e.g. if parsing the
    // beginning of an ETL file where we can get packet events before the
    // context mapping.
    auto ii = mContexts.find(hContext);
    if (ii == mContexts.end()) {
        return 0;
    }
    auto context = &ii->second;

    // Should not see any dma packets on a HwQueue
    DebugAssert(!context->mIsHwQueue);

    // Stop tracking the work
    if (!CompleteWork(context, sequenceId, timestamp)) {
        return 0;
    }

    // Return the non-zero cloud streaming process id if this is the end of an
    // identified video encode packet on a context used for cloud streaming.
    return context->mIsVideoEncoderForCloudStreamingApp
        ? mCloudStreamingProcessId
        : 0;
}

void GpuTrace::StartINTCTimer(INTCGPUTimer timer, uint32_t processId, uint64_t timestamp)
{
    auto frameInfo = &mProcessFrameInfo.emplace(processId, ProcessFrameInfo{}).first->second;
    auto timerInfo = &frameInfo->mINTCTimers[timer];

    timerInfo->mStartCount += 1;
    if (timerInfo->mStartCount == 1) {
        timerInfo->mStartTime = timestamp;
    }
}

void GpuTrace::StopINTCTimer(INTCGPUTimer timer, uint32_t processId, uint64_t timestamp)
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

        // mStartTime should be 0, if it isn't then either a
        // PagingQueuePacket_Stop event was missed (which is expected but
        // should be rare) or multiple paging packets can execute in parallel
        // (which violates the assumption we're making).  If the latter, this
        // needs to be fixed.
        if (timer->mStartTime == 0) {
            timer->mStartTime = timestamp;
        }
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
    // There are a few different events that can be used to complete the GPU
    // trace for each frame, e.g. QueuePacket_Stop for a present packet, or
    // MMIOFlipMultiPlaneOverlay_Info, and they can occur in any order so we
    // only apply the first one we see.
    if (pEvent->GpuFrameCompleted) {
        return;
    }

    VerboseTraceBeforeModifyingPresent(pEvent);
    pEvent->GpuFrameCompleted = true;

    auto ii = mProcessFrameInfo.find(pEvent->ProcessId);
    if (ii != mProcessFrameInfo.end()) {
        auto frameInfo = &ii->second;
        auto packetTrace = &frameInfo->mOtherEngines;
        auto videoTrace = &frameInfo->mVideoEngines;

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

        if (mPMConsumer->mTrackINTCTimers || mPMConsumer->mTrackINTCCpuGpuSync || mPMConsumer->mTrackINTCShaderCompilation) {
            for (uint32_t i = 0; i < INTC_GPU_TIMER_COUNT; ++i) {
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

        if (IsVerboseTraceEnabled()) {
            printf("                             GPU: pid=%u completing frame\n", pEvent->ProcessId);
        }

        // There are some cases where the QueuePacket_Stop timestamp is before
        // the previous dma packet completes.  e.g., this seems to be typical
        // of DWM present packets.  In these cases, instead of loosing track of
        // the previous dma work, we split it at this time and assign portions
        // to both frames.  Note this is incorrect, as the dma's full cost
        // should be fully attributed to the previous frame.
        if (packetTrace->mRunningPacketCount > 0) {

            pEvent->ReadyTime = timestamp;

            auto accumulatedTime = timestamp - packetTrace->mRunningPacketStartTime;
            if (accumulatedTime > 0) {
                if (IsVerboseTraceEnabled()) {
                    DebugPrintAccumulatedGpuTime(pEvent->ProcessId,
                                                 pEvent->GPUDuration,
                                                 packetTrace->mRunningPacketStartTime,
                                                 timestamp);
                    printf("                             GPU: work still running; splitting and considering as new work for next frame\n");
                }

                pEvent->GPUDuration += accumulatedTime;
                packetTrace->mFirstPacketTime = timestamp;
                packetTrace->mRunningPacketStartTime = timestamp;
            }
        }
        if (videoTrace->mRunningPacketCount > 0) {
            pEvent->GPUVideoDuration += timestamp - videoTrace->mRunningPacketStartTime;
            videoTrace->mFirstPacketTime = timestamp;
            videoTrace->mRunningPacketStartTime = timestamp;
        }
    }

    // If we did not track any GPU work, use the frame completion time as the
    // ready time.
    if (pEvent->ReadyTime == 0) {
        pEvent->ReadyTime = timestamp;
    }
}
