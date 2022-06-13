// Copyright (C) 2017-2022 Intel Corporation
// SPDX-License-Identifier: MIT

#include "PresentMonTraceConsumer.hpp"

#include "ETW/Intel_Graphics_D3D10.h"
#include "ETW/Intel_PCAT_Metrics.h"
#include "ETW/Microsoft_Windows_D3D9.h"
#include "ETW/Microsoft_Windows_Dwm_Core.h"
#include "ETW/Microsoft_Windows_DXGI.h"
#include "ETW/Microsoft_Windows_DxgKrnl.h"
#include "ETW/Microsoft_Windows_EventMetadata.h"
#include "ETW/Microsoft_Windows_Win32k.h"

#include <algorithm>
#include <assert.h>
#include <d3d9.h>
#include <dxgi.h>
#include <unordered_set>

#ifdef DEBUG
static constexpr int PRESENTEVENT_CIRCULAR_BUFFER_SIZE = 32768;
#else
static constexpr int PRESENTEVENT_CIRCULAR_BUFFER_SIZE = 8192;
#endif

namespace {

// Detect if there are any missing expected events, and returns the number of
// PresentStop events that we should wait for them.
uint32_t GetDeferredCompletionWaitCount(PresentEvent const& p)
{
    // If the present recieved a INTC_FrameID, but hasn't seen a
    // task_FramePacer_Info event yet, we may need to wait for up to 16
    // presents to see it.
    if (p.INTC_FrameID > 0 && p.SeenINTCFramePacerInfo == false) {
        return 16;
    }

    // If the present was displayed or discarded before Present_Stop, defer
    // completion for for one Present_Stop.
    if (p.Runtime != Runtime::Other && p.TimeTaken == 0) {
        return 1;
    }

    // All expected events already observed
    return 0;
}

}

// These macros, when enabled, record what PresentMon analysis below was done
// for each present.  The primary use case is to compute usage statistics and
// ensure test coverage.
//
// Add a TRACK_PRESENT_PATH() calls to every location that represents a unique
// analysis path.  e.g., as a starting point this might be one for every ETW
// event used below, with further instrumentation when there is different
// handling based on event property values.
//
// If the location is in a function that can be called by multiple parents, use
// TRACK_PRESENT_SAVE_PATH_ID() instead and call
// TRACK_PRESENT_GENERATE_PATH_ID() in each parent.
#ifdef TRACK_PRESENT_PATHS
#define TRACK_PRESENT_PATH_SAVE_ID(present, id) present->AnalysisPath |= 1ull << (id % 64)
#define TRACK_PRESENT_PATH(present) do { \
    enum { TRACK_PRESENT_PATH_ID = __COUNTER__ }; \
    TRACK_PRESENT_PATH_SAVE_ID(present, TRACK_PRESENT_PATH_ID); \
} while (0)
#define TRACK_PRESENT_PATH_GENERATE_ID()              mAnalysisPathID = __COUNTER__
#define TRACK_PRESENT_PATH_SAVE_GENERATED_ID(present) TRACK_PRESENT_PATH_SAVE_ID(present, mAnalysisPathID)
#else
#define TRACK_PRESENT_PATH(present)                   (void) present
#define TRACK_PRESENT_PATH_GENERATE_ID()
#define TRACK_PRESENT_PATH_SAVE_GENERATED_ID(present) (void) present
#endif

PresentEvent::PresentEvent(EVENT_HEADER const& hdr, ::Runtime runtime)
    : QpcTime(*(uint64_t*) &hdr.TimeStamp)
    , ProcessId(hdr.ProcessId)
    , ThreadId(hdr.ThreadId)
    , TimeTaken(0)
    , GPUStartTime(0)
    , ReadyTime(0)
    , GPUDuration(0)
    , GPUVideoDuration(0)
    , ScreenTime(0)
    , InputTime(0)

    , SwapChainAddress(0)
    , SyncInterval(-1)
    , PresentFlags(0)

    , INTC_ProducerPresentTime(0)
    , INTC_ConsumerPresentTime(0)

    , INTC_FrameID(0)
    , INTC_AppWorkStart(0)
    , INTC_AppSimulationTime(0)
    , INTC_DriverWorkStart(0)
    , INTC_DriverWorkEnd(0)
    , INTC_KernelDriverSubmitStart(0)
    , INTC_KernelDriverSubmitEnd(0)
    , INTC_GPUStart(0)
    , INTC_GPUEnd(0)
    , INTC_KernelDriverFenceReport(0)
    , INTC_PresentAPICall(0)
    , INTC_TargetFrameTime(0)
    , INTC_FlipReceivedTime(0)
    , INTC_FlipReportTime(0)
    , INTC_FlipProgrammingTime(0)
    , INTC_ActualFlipTime(0)

    , CompositionSurfaceLuid(0)
    , Win32KPresentCount(0)
    , Win32KBindId(0)
    , DxgkPresentHistoryToken(0)
    , DxgkPresentHistoryTokenData(0)
    , DxgkContext(0)
    , Hwnd(0)
    , mAllPresentsTrackingIndex(UINT32_MAX)
    , QueueSubmitSequence(0)

    , DeferredCompletionWaitCount(0)

    , DestWidth(0)
    , DestHeight(0)
    , DriverBatchThreadId(0)

    , Runtime(runtime)
    , PresentMode(PresentMode::Unknown)
    , FinalState(PresentResult::Unknown)
    , InputType(InputDeviceType::Unknown)

    , SupportsTearing(false)
    , MMIO(false)
    , SeenDxgkPresent(false)
    , SeenWin32KEvents(false)
    , SeenINTCFramePacerInfo(false)
    , DwmNotified(false)
    , SeenInFrameEvent(false)
    , IsCompleted(false)
    , IsLost(false)
    , PresentInDwmWaitingStruct(false)
{
#ifdef TRACK_PRESENT_PATHS
    AnalysisPath = 0ull;
#endif

#if DEBUG_VERBOSE
    static uint64_t presentCount = 0;
    presentCount += 1;
    Id = presentCount;
#endif

    memset(INTC_Timers, 0, sizeof(INTC_Timers));
    memset(MemoryResidency, 0, sizeof(MemoryResidency));
}

PMTraceConsumer::PMTraceConsumer()
    : mAllPresents(PRESENTEVENT_CIRCULAR_BUFFER_SIZE)
    , mGpuTrace(this)
    , mLastInputDeviceReadTime(0)
    , mLastInputDeviceType(InputDeviceType::Unknown)
{
}

void PMTraceConsumer::HandleIntelGraphicsEvent(EVENT_RECORD* pEventRecord)
{
    DebugEvent(pEventRecord, &mMetadata);

    auto const& hdr = pEventRecord->EventHeader;
    switch (hdr.EventDescriptor.Id) {
    case Intel_Graphics_D3D10::QueueTimers_Info::Id:
    {
        auto Type = mMetadata.GetEventData<Intel_Graphics_D3D10::mTimerType>(pEventRecord, L"value");
        switch (Type) {
        case Intel_Graphics_D3D10::mTimerType::FRAME_TIME_APP:    mGpuTrace.SetINTCProducerPresentTime(hdr.ProcessId, hdr.TimeStamp.QuadPart); break;
        case Intel_Graphics_D3D10::mTimerType::FRAME_TIME_DRIVER: mGpuTrace.SetINTCConsumerPresentTime(hdr.ProcessId, hdr.TimeStamp.QuadPart); break;
        default: DebugAssert(false); break;
        }
        break;
    }

    case Intel_Graphics_D3D10::QueueTimers_Start::Id:
    case Intel_Graphics_D3D10::QueueTimers_Stop::Id:
    case Intel_Graphics_D3D10::CpuGpuSync_Start::Id:
    case Intel_Graphics_D3D10::CpuGpuSync_Stop::Id:
    case Intel_Graphics_D3D10::ShaderCompilationTrackingEvents_Start_3::Id:
    case Intel_Graphics_D3D10::ShaderCompilationTrackingEvents_Stop_3::Id:
    case Intel_Graphics_D3D10::ShaderCompilationTrackingEvents_Start_4::Id:
    case Intel_Graphics_D3D10::ShaderCompilationTrackingEvents_Stop_4::Id:
    {
        INTCTimer timer = INTC_TIMER_COUNT;

        switch (hdr.EventDescriptor.Id) {
        case Intel_Graphics_D3D10::QueueTimers_Start::Id:
        case Intel_Graphics_D3D10::QueueTimers_Stop::Id:
        {
            auto Type = mMetadata.GetEventData<Intel_Graphics_D3D10::mTimerType>(pEventRecord, L"value");
            switch (Type) {
            case Intel_Graphics_D3D10::mTimerType::WAIT_IF_FULL_TIMER:           timer = INTC_TIMER_WAIT_IF_FULL; break;
            case Intel_Graphics_D3D10::mTimerType::WAIT_IF_EMPTY_TIMER:          timer = INTC_TIMER_WAIT_IF_EMPTY; break;
            case Intel_Graphics_D3D10::mTimerType::WAIT_UNTIL_EMPTY_SYNC_TIMER:  timer = INTC_TIMER_WAIT_UNTIL_EMPTY_SYNC; break;
            case Intel_Graphics_D3D10::mTimerType::WAIT_UNTIL_EMPTY_DRAIN_TIMER: timer = INTC_TIMER_WAIT_UNTIL_EMPTY_DRAIN; break;
            case Intel_Graphics_D3D10::mTimerType::WAIT_FOR_FENCE:               timer = INTC_TIMER_WAIT_FOR_FENCE; break;
            case Intel_Graphics_D3D10::mTimerType::WAIT_UNTIL_FENCE_SUBMITTED:   timer = INTC_TIMER_WAIT_UNTIL_FENCE_SUBMITTED; break;
            default: DebugAssert(false); break;
            }
            break;
        }

        case Intel_Graphics_D3D10::CpuGpuSync_Start::Id:
        case Intel_Graphics_D3D10::CpuGpuSync_Stop::Id:
        {
            auto Type = mMetadata.GetEventData<Intel_Graphics_D3D10::mSyncType>(pEventRecord, L"value");
            switch (Type) {
            case Intel_Graphics_D3D10::mSyncType::SYNC_TYPE_WAIT_SYNC_OBJECT_CPU:   timer = INTC_TIMER_SYNC_TYPE_WAIT_SYNC_OBJECT_CPU; break;
            case Intel_Graphics_D3D10::mSyncType::SYNC_TYPE_POLL_ON_QUERY_GET_DATA: timer = INTC_TIMER_SYNC_TYPE_POLL_ON_QUERY_GET_DATA; break;
            default: DebugAssert(false); break;
            }
            break;
        }

        case Intel_Graphics_D3D10::ShaderCompilationTrackingEvents_Start_3::Id:
        case Intel_Graphics_D3D10::ShaderCompilationTrackingEvents_Stop_3::Id:
            timer = INTC_TIMER_WAIT_FOR_COMPILATION_ON_DRAW;
            break;

        case Intel_Graphics_D3D10::ShaderCompilationTrackingEvents_Start_4::Id:
        case Intel_Graphics_D3D10::ShaderCompilationTrackingEvents_Stop_4::Id:
            timer = INTC_TIMER_WAIT_FOR_COMPILATION_ON_CREATE;
            break;
        }

        switch (hdr.EventDescriptor.Id) {
        case Intel_Graphics_D3D10::QueueTimers_Start::Id:
        case Intel_Graphics_D3D10::CpuGpuSync_Start::Id:
        case Intel_Graphics_D3D10::ShaderCompilationTrackingEvents_Start_3::Id:
        case Intel_Graphics_D3D10::ShaderCompilationTrackingEvents_Start_4::Id:
            mGpuTrace.StartINTCTimer(timer, hdr.ProcessId, hdr.TimeStamp.QuadPart);
            break;
        case Intel_Graphics_D3D10::QueueTimers_Stop::Id:
        case Intel_Graphics_D3D10::CpuGpuSync_Stop::Id:
        case Intel_Graphics_D3D10::ShaderCompilationTrackingEvents_Stop_3::Id:
        case Intel_Graphics_D3D10::ShaderCompilationTrackingEvents_Stop_4::Id:
            mGpuTrace.StopINTCTimer(timer, hdr.ProcessId, hdr.TimeStamp.QuadPart);
            break;
        }
        break;
    }

    // Provides a driver frame ID used to associate task_FramePacer_Info events
    // which occur some time after display.
    //
    // Should occur between Microsoft_Windows_DXGI::Present_Start and
    // Microsoft_Windows_DXGI::Present_Stop on the same thread.
    case Intel_Graphics_D3D10::task_DdiPresentDXGI_Info::Id:
    {
        DebugAssert(mDebugINTCFramePacing);
        EventDataDesc desc[] = {
            { L"FrameID" },
        };
        /* BEGIN WORKAROUND: if the manifest isn't installed nor embedded
         * into the ETL this won't be able to lookup properties
        mMetadata.GetEventData(pEventRecord, desc, _countof(desc));
        */
        mMetadata.GetEventData(pEventRecord, desc, _countof(desc), _countof(desc));
        if ((desc[0].status_ & PROP_STATUS_FOUND) == 0) break;
        /* END WORKAROUND */
        auto FrameID = desc[0].GetData<uint64_t>();

        auto present = FindOrCreatePresent(hdr);
        DebugModifyPresent(present.get());
        present->INTC_FrameID = FrameID;
        break;
    }

    // Provides application/driver-provided information about an
    // already-displayed frame.
    //
    // Should occur between Microsoft_Windows_DXGI::Present_Start and
    // Microsoft_Windows_DXGI::Present_Stop on the same thread.
    case Intel_Graphics_D3D10::task_FramePacer_Info::Id:
    {
        DebugAssert(mDebugINTCFramePacing);
        EventDataDesc desc[] = {
            { L"FrameID" },
            { L"AppWorkStart" },
            { L"AppSimulationTime" },
            { L"DriverWorkStart" },
            { L"DriverWorkEnd" },
            { L"KernelDriverSubmitStart" },
            { L"KernelDriverSubmitEnd" },
            { L"GPUStart" },
            { L"GPUEnd" },
            { L"KernelDriverFenceReport" },
            { L"PresentAPICall" },
            { L"TargetFrameTime" },
            { L"FlipReceivedTime" },
            { L"FlipReportTime" },
            { L"FlipProgrammingTime" },
            { L"ActualFlipTime" },
        };
        /* BEGIN WORKAROUND: if the manifest isn't installed nor embedded
         * into the ETL this won't be able to lookup properties
        mMetadata.GetEventData(pEventRecord, desc, _countof(desc));
        */
        mMetadata.GetEventData(pEventRecord, desc, _countof(desc), _countof(desc));
        if ((desc[0].status_ & PROP_STATUS_FOUND) == 0) break;
        /* END WORKAROUND */
        auto FrameID                 = desc[0].GetData<uint64_t>();
        auto AppWorkStart            = desc[1].GetData<uint64_t>();
        auto AppSimulationTime       = desc[2].GetData<uint64_t>();
        auto DriverWorkStart         = desc[3].GetData<uint64_t>();
        auto DriverWorkEnd           = desc[4].GetData<uint64_t>();
        auto KernelDriverSubmitStart = desc[5].GetData<uint64_t>();
        auto KernelDriverSubmitEnd   = desc[6].GetData<uint64_t>();
        auto GPUStart                = desc[7].GetData<uint64_t>();
        auto GPUEnd                  = desc[8].GetData<uint64_t>();
        auto KernelDriverFenceReport = desc[9].GetData<uint64_t>();
        auto PresentAPICall          = desc[10].GetData<uint64_t>();
        auto TargetFrameTime         = desc[11].GetData<uint64_t>();
        auto FlipReceivedTime        = desc[12].GetData<uint64_t>();
        auto FlipReportTime          = desc[13].GetData<uint64_t>();
        auto FlipProgrammingTime     = desc[14].GetData<uint64_t>();
        auto ActualFlipTime          = desc[15].GetData<uint64_t>();

        // Search for a present with the same frame ID.  Search the deferred
        // completions first, as that is the most likely location since we get
        // task_framePacer_Info events late.  Then, if not found, check
        // in-progress presents.
        std::shared_ptr<PresentEvent> present;
        {
            auto iter = mDeferredCompletions.find(hdr.ProcessId);
            if (iter != mDeferredCompletions.end()) {
                for (auto const& pr1 : iter->second) {
                    for (auto const& pr2 : pr1.second.mOrderedPresents) {
                        if (pr2.second->INTC_FrameID == FrameID) {
                            present = pr2.second;
                            break;
                        }
                    }
                }
            }
        }
        if (present == nullptr) {
            auto iter = mOrderedPresentsByProcessId.find(hdr.ProcessId);
            if (iter != mOrderedPresentsByProcessId.end()) {
                for (auto const& pr : iter->second) {
                    if (pr.second->INTC_FrameID == FrameID) {
                        present = pr.second;
                        break;
                    }
                }
            }
        }

        // If found, add the frame pacer info.
        if (present != nullptr) {
            DebugModifyPresent(present.get());
            present->INTC_AppWorkStart            = AppWorkStart;
            present->INTC_AppSimulationTime       = AppSimulationTime;
            present->INTC_DriverWorkStart         = DriverWorkStart;
            present->INTC_DriverWorkEnd           = DriverWorkEnd;
            present->INTC_KernelDriverSubmitStart = KernelDriverSubmitStart;
            present->INTC_KernelDriverSubmitEnd   = KernelDriverSubmitEnd;
            present->INTC_GPUStart                = GPUStart;
            present->INTC_GPUEnd                  = GPUEnd;
            present->INTC_KernelDriverFenceReport = KernelDriverFenceReport;
            present->INTC_PresentAPICall          = PresentAPICall;
            present->INTC_TargetFrameTime         = TargetFrameTime;
            present->INTC_FlipReceivedTime        = FlipReceivedTime;
            present->INTC_FlipReportTime          = FlipReportTime;
            present->INTC_FlipProgrammingTime     = FlipProgrammingTime;
            present->INTC_ActualFlipTime          = ActualFlipTime;
            present->SeenINTCFramePacerInfo       = true;

            // If the present was deferred waiting for this event, enqueue it now.
            if (present->DeferredCompletionWaitCount > 0 && GetDeferredCompletionWaitCount(*present) == 0) {
                present->DeferredCompletionWaitCount = 0;
                EnqueueDeferredPresent(present);
            }
        }
        break;
    }

    default:
        assert(!mFilteredEvents); // Assert that filtering is working if expected
        break;
    }
}

void PMTraceConsumer::HandleIntelPCATEvent(EVENT_RECORD* pEventRecord)
{
    DebugEvent(pEventRecord, &mMetadata);

    auto const& hdr = pEventRecord->EventHeader;
    switch (hdr.EventDescriptor.Id) {

    case Intel_PCAT_Metrics::Task_0_Opcode_0::Id:
    {
        static FILE* powerCsv = nullptr;
        if (powerCsv == nullptr) {
            static bool powerCsvFailed = false;
            if (powerCsvFailed) return;
            if (fopen_s(&powerCsv, "presentmon_pcat.csv", "wb")) {
                fprintf(stderr, "error: failed to create file: presentmon_pcat.csv\n");
                powerCsvFailed = true;
                return;
            }

            fprintf(powerCsv, "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n",
                "frequency",
                "system_tick",
                "timestamp",
                "p1_12v_8p_v",
                "p1_12v_8p_a",
                "p2_12v_8p_v",
                "p2_12v_8p_a",
                "p3_12v_8p_v",
                "p3_12v_8p_a",
                "pcie_12v_v",
                "pcie_12v_a",
                "three_point_three_v",
                "three_point_three_a",
                "three_point_three_aux_v",
                "three_point_three_aux_a");
        }

        auto p = (Intel_PCAT_Metrics::Task_0_Opcode_0_Struct*) pEventRecord->UserData;
        fprintf(powerCsv, "%lld,%lld,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\n",
            p->frequency,
            p->system_tick,
            p->timestamp,
            p->p1_12v_8p_v,
            p->p1_12v_8p_a,
            p->p2_12v_8p_v,
            p->p2_12v_8p_a,
            p->p3_12v_8p_v,
            p->p3_12v_8p_a,
            p->pcie_12v_v,
            p->pcie_12v_a,
            p->three_point_three_v,
            p->three_point_three_a,
            p->three_point_three_aux_v,
            p->three_point_three_aux_a);
        break;
    }

    default:
        assert(!mFilteredEvents); // Assert that filtering is working if expected
        break;
    }
}

void PMTraceConsumer::HandleD3D9Event(EVENT_RECORD* pEventRecord)
{
    DebugEvent(pEventRecord, &mMetadata);

    auto const& hdr = pEventRecord->EventHeader;

    if (!IsProcessTrackedForFiltering(hdr.ProcessId)) {
        return;
    }

    switch (hdr.EventDescriptor.Id) {
    case Microsoft_Windows_D3D9::Present_Start::Id:
    {
        EventDataDesc desc[] = {
            { L"pSwapchain" },
            { L"Flags" },
        };
        mMetadata.GetEventData(pEventRecord, desc, _countof(desc));
        auto pSwapchain = desc[0].GetData<uint64_t>();
        auto Flags      = desc[1].GetData<uint32_t>();

        uint32_t dxgiPresentFlags = 0;
        if (Flags & D3DPRESENT_DONOTFLIP)   dxgiPresentFlags |= DXGI_PRESENT_DO_NOT_SEQUENCE;
        if (Flags & D3DPRESENT_DONOTWAIT)   dxgiPresentFlags |= DXGI_PRESENT_DO_NOT_WAIT;
        if (Flags & D3DPRESENT_FLIPRESTART) dxgiPresentFlags |= DXGI_PRESENT_RESTART;

        int32_t syncInterval = -1;
        if (Flags & D3DPRESENT_FORCEIMMEDIATE) {
            syncInterval = 0;
        }

        TRACK_PRESENT_PATH_GENERATE_ID();

        RuntimePresentStart(Runtime::D3D9, hdr, pSwapchain, dxgiPresentFlags, syncInterval);
        break;
    }
    case Microsoft_Windows_D3D9::Present_Stop::Id:
        RuntimePresentStop(Runtime::D3D9, hdr, mMetadata.GetEventData<uint32_t>(pEventRecord, L"Result"));
        break;
    default:
        assert(!mFilteredEvents); // Assert that filtering is working if expected
        break;
    }
}

void PMTraceConsumer::HandleDXGIEvent(EVENT_RECORD* pEventRecord)
{
    DebugEvent(pEventRecord, &mMetadata);

    auto const& hdr = pEventRecord->EventHeader;

    if (!IsProcessTrackedForFiltering(hdr.ProcessId)) {
        return;
    }

    switch (hdr.EventDescriptor.Id) {
    case Microsoft_Windows_DXGI::Present_Start::Id:
    case Microsoft_Windows_DXGI::PresentMultiplaneOverlay_Start::Id:
    {
        EventDataDesc desc[] = {
            { L"pIDXGISwapChain" },
            { L"Flags" },
            { L"SyncInterval" },
        };
        mMetadata.GetEventData(pEventRecord, desc, _countof(desc));
        auto pSwapChain   = desc[0].GetData<uint64_t>();
        auto Flags        = desc[1].GetData<uint32_t>();
        auto SyncInterval = desc[2].GetData<int32_t>();

        TRACK_PRESENT_PATH_GENERATE_ID();

        RuntimePresentStart(Runtime::DXGI, hdr, pSwapChain, Flags, SyncInterval);
        break;
    }
    case Microsoft_Windows_DXGI::Present_Stop::Id:
    case Microsoft_Windows_DXGI::PresentMultiplaneOverlay_Stop::Id:
        RuntimePresentStop(Runtime::DXGI, hdr, mMetadata.GetEventData<uint32_t>(pEventRecord, L"Result"));
        break;
    default:
        assert(!mFilteredEvents); // Assert that filtering is working if expected
        break;
    }
}

void PMTraceConsumer::HandleDxgkBlt(EVENT_HEADER const& hdr, uint64_t hwnd, bool redirectedPresent)
{
    // Lookup the in-progress present.
    auto presentEvent = FindOrCreatePresent(hdr);
    if (presentEvent == nullptr) {
        return;
    }

    // The looked-up present should not have a known present mode yet.  If it
    // does, we looked up a present whose tracking was lost for some reason.
    if (presentEvent->PresentMode != PresentMode::Unknown) {
        RemoveLostPresent(presentEvent);
        presentEvent = FindOrCreatePresent(hdr);
        if (presentEvent == nullptr) {
            return;
        }
        DebugAssert(presentEvent->PresentMode == PresentMode::Unknown);
    }

    TRACK_PRESENT_PATH_SAVE_GENERATED_ID(presentEvent);

    // This could be one of several types of presents. Further events will clarify.
    // For now, assume that this is a blt straight into a surface which is already on-screen.
    presentEvent->Hwnd = hwnd;
    if (redirectedPresent) {
        TRACK_PRESENT_PATH(presentEvent);
        presentEvent->PresentMode = PresentMode::Composed_Copy_CPU_GDI;
        presentEvent->SupportsTearing = false;
    } else {
        presentEvent->PresentMode = PresentMode::Hardware_Legacy_Copy_To_Front_Buffer;
        presentEvent->SupportsTearing = true;
    }
}

void PMTraceConsumer::HandleDxgkBltCancel(EVENT_HEADER const& hdr)
{
    // There are cases where a present blt can be optimized out in kernel.
    // In such cases, we return success to the caller, but issue no further work
    // for the present. Mark these cases as discarded.
    auto eventIter = mPresentByThreadId.find(hdr.ThreadId);
    if (eventIter != mPresentByThreadId.end()) {
        auto present = eventIter->second;

        TRACK_PRESENT_PATH(present);
        DebugModifyPresent(present.get());
        present->FinalState = PresentResult::Discarded;

        CompletePresent(present);
    }
}

void PMTraceConsumer::HandleDxgkFlip(EVENT_HEADER const& hdr, int32_t flipInterval, bool mmio)
{
    // A flip event is emitted during fullscreen present submission.
    // Afterwards, expect an MMIOFlip packet on the same thread, used to trace
    // the flip to screen.

    // Lookup the in-progress present.
    auto presentEvent = FindOrCreatePresent(hdr);
    if (presentEvent == nullptr) {
        return;
    }

    // The only events that we expect before a Flip/FlipMPO are a runtime
    // present start, or a previous FlipMPO.  If that's not the case, we assume
    // that correct tracking has been lost.
    while (presentEvent->QueueSubmitSequence != 0 || presentEvent->SeenDxgkPresent) {
        RemoveLostPresent(presentEvent);
        presentEvent = FindOrCreatePresent(hdr);
        if (presentEvent == nullptr) {
            return;
        }
    }

    TRACK_PRESENT_PATH_SAVE_GENERATED_ID(presentEvent);

    // For MPO, N events may be issued, but we only care about the first
    if (presentEvent->PresentMode != PresentMode::Unknown) {
        return;
    }

    DebugModifyPresent(presentEvent.get());
    presentEvent->MMIO = mmio;
    presentEvent->PresentMode = PresentMode::Hardware_Legacy_Flip;

    if (flipInterval != -1) {
        presentEvent->SyncInterval = flipInterval;
    }
    if (!mmio) {
        presentEvent->SupportsTearing = flipInterval == 0;
    }

    // If this is the DWM thread, piggyback these pending presents on our fullscreen present
    if (hdr.ThreadId == DwmPresentThreadId) {
        for (auto iter = mPresentsWaitingForDWM.begin(); iter != mPresentsWaitingForDWM.end(); iter++) {
            iter->get()->PresentInDwmWaitingStruct = false;
        }
        std::swap(presentEvent->DependentPresents, mPresentsWaitingForDWM);
        DwmPresentThreadId = 0;
    }
}

template<bool Win7>
void PMTraceConsumer::HandleDxgkQueueSubmit(
    EVENT_HEADER const& hdr,
    uint64_t hContext,
    uint32_t submitSequence,
    uint32_t packetType,
    bool isPresentPacket)
{
    // Track GPU execution
    if (mTrackGPU) {
        mGpuTrace.EnqueueQueuePacket(hdr.ProcessId, hContext);
    }

    // For blt presents on Win7, the only way to distinguish between DWM-off
    // fullscreen blts and the DWM-on blt to redirection bitmaps is to track
    // whether a PHT was submitted before submitting anything else to the same
    // context, which indicates it's a redirected blt.  If we get to here
    // instead, the present is a fullscreen blt and considered completed once
    // its work is done.
    #pragma warning(suppress: 4984) // C++17 language extension
    if constexpr (Win7) {
        auto eventIter = mPresentByDxgkContext.find(hContext);
        if (eventIter != mPresentByDxgkContext.end()) {
            auto present = eventIter->second;

            TRACK_PRESENT_PATH(present);

            if (present->PresentMode == PresentMode::Hardware_Legacy_Copy_To_Front_Buffer) {
                DebugModifyPresent(present.get());
                present->SeenDxgkPresent = true;

                // If the work is already done, complete it now.
                if (present->ScreenTime != 0) {
                    CompletePresent(present);
                }
            }

            // We're done with DxgkContext tracking, if the present hasn't
            // completed remove it from the tracking now.
            if (present->DxgkContext != 0) {
                mPresentByDxgkContext.erase(eventIter);
                present->DxgkContext = 0;
            }
        }
    }

    // This event is emitted after a flip/blt/PHT event, and may be the only
    // way to trace completion of the present.
    if (packetType == (uint32_t) Microsoft_Windows_DxgKrnl::QueuePacketType::DXGKETW_MMIOFLIP_COMMAND_BUFFER ||
        packetType == (uint32_t) Microsoft_Windows_DxgKrnl::QueuePacketType::DXGKETW_SOFTWARE_COMMAND_BUFFER ||
        isPresentPacket) {
        auto eventIter = mPresentByThreadId.find(hdr.ThreadId);
        if (eventIter != mPresentByThreadId.end()) {
            auto present = eventIter->second;

            if (present->QueueSubmitSequence == 0) {

                TRACK_PRESENT_PATH(present);
                DebugModifyPresent(present.get());

                present->QueueSubmitSequence = submitSequence;
                mPresentBySubmitSequence.emplace(submitSequence, present);

                #pragma warning(suppress: 4984) // C++17 language extension
                if constexpr (Win7) {
                    if (present->PresentMode == PresentMode::Hardware_Legacy_Copy_To_Front_Buffer) {
                        mPresentByDxgkContext[hContext] = present;
                        present->DxgkContext = hContext;
                    }
                }
            }
        }
    }
}

void PMTraceConsumer::HandleDxgkQueueComplete(EVENT_HEADER const& hdr, uint32_t submitSequence)
{
    // Check if this is a present Packet being tracked...
    auto eventIter = mPresentBySubmitSequence.find(submitSequence);
    if (eventIter == mPresentBySubmitSequence.end()) {
        return;
    }
    auto pEvent = eventIter->second;

    TRACK_PRESENT_PATH_SAVE_GENERATED_ID(pEvent);

    // Assign any tracked accumulated GPU work to the present.
    if (mTrackGPU) {
        mGpuTrace.CompleteFrame(pEvent.get(), hdr.TimeStamp.QuadPart);
    }

    // If this is one of the present modes for which packet completion implies
    // display, then complete the present now.
    if (pEvent->PresentMode == PresentMode::Hardware_Legacy_Copy_To_Front_Buffer ||
        (pEvent->PresentMode == PresentMode::Hardware_Legacy_Flip && !pEvent->MMIO)) {
        DebugModifyPresent(pEvent.get());
        pEvent->ReadyTime = hdr.TimeStamp.QuadPart;
        pEvent->ScreenTime = hdr.TimeStamp.QuadPart;
        pEvent->FinalState = PresentResult::Presented;

        // Sometimes, the queue packets associated with a present will complete
        // before the DxgKrnl PresentInfo event is fired.  For blit presents in
        // this case, we have no way to differentiate between fullscreen and
        // windowed blits, so we defer the completion of this present until
        // we've also seen the Dxgk Present_Info event.
        if (pEvent->SeenDxgkPresent || pEvent->PresentMode != PresentMode::Hardware_Legacy_Copy_To_Front_Buffer) {
            CompletePresent(pEvent);
        }
    }
}

// An MMIOFlip event is emitted when an MMIOFlip packet is dequeued.  All GPU
// work submitted prior to the flip has been completed.
//
// It also is emitted when an independent flip PHT is dequed, and will tell us
// whether the present is immediate or vsync.
void PMTraceConsumer::HandleDxgkMMIOFlip(EVENT_HEADER const& hdr, uint32_t flipSubmitSequence, uint32_t flags)
{
    auto eventIter = mPresentBySubmitSequence.find(flipSubmitSequence);
    if (eventIter == mPresentBySubmitSequence.end()) {
        return;
    }
    auto pEvent = eventIter->second;

    TRACK_PRESENT_PATH_SAVE_GENERATED_ID(pEvent);

    DebugModifyPresent(pEvent.get());
    pEvent->ReadyTime = hdr.TimeStamp.QuadPart;

    if (pEvent->PresentMode == PresentMode::Composed_Flip) {
        pEvent->PresentMode = PresentMode::Hardware_Independent_Flip;
    }

    if (flags & (uint32_t) Microsoft_Windows_DxgKrnl::SetVidPnSourceAddressFlags::FlipImmediate) {
        pEvent->FinalState = PresentResult::Presented;
        pEvent->ScreenTime = hdr.TimeStamp.QuadPart;
        pEvent->SupportsTearing = true;
        if (pEvent->PresentMode == PresentMode::Hardware_Legacy_Flip) {
            CompletePresent(pEvent);
        }
    }
}

void PMTraceConsumer::HandleDxgkMMIOFlipMPO(EVENT_HEADER const& hdr, uint32_t flipSubmitSequence,
                                            uint32_t flipEntryStatusAfterFlip, bool flipEntryStatusAfterFlipValid)
{
    auto eventIter = mPresentBySubmitSequence.find(flipSubmitSequence);
    if (eventIter == mPresentBySubmitSequence.end()) {
        return;
    }
    auto pEvent = eventIter->second;

    TRACK_PRESENT_PATH(pEvent);

    // Avoid double-marking a single present packet coming from the MPO API
    if (pEvent->ReadyTime == 0) {
        DebugModifyPresent(pEvent.get());
        pEvent->ReadyTime = hdr.TimeStamp.QuadPart;
    }

    if (!flipEntryStatusAfterFlipValid) {
        return;
    }

    TRACK_PRESENT_PATH(pEvent);

    // Present could tear if we're not waiting for vsync
    if (flipEntryStatusAfterFlip != (uint32_t) Microsoft_Windows_DxgKrnl::FlipEntryStatus::FlipWaitVSync) {
        DebugModifyPresent(pEvent.get());
        pEvent->SupportsTearing = true;
    }

    // For the VSync ahd HSync paths, we'll wait for the corresponding ?SyncDPC
    // event before considering the present complete to get a more-accurate
    // ScreenTime (see HandleDxgkSyncDPC).
    if (flipEntryStatusAfterFlip == (uint32_t) Microsoft_Windows_DxgKrnl::FlipEntryStatus::FlipWaitVSync ||
        flipEntryStatusAfterFlip == (uint32_t) Microsoft_Windows_DxgKrnl::FlipEntryStatus::FlipWaitHSync) {
        return;
    }

    TRACK_PRESENT_PATH(pEvent);

    DebugModifyPresent(pEvent.get());
    pEvent->FinalState = PresentResult::Presented;
    if (flipEntryStatusAfterFlip == (uint32_t) Microsoft_Windows_DxgKrnl::FlipEntryStatus::FlipWaitComplete) {
        pEvent->ScreenTime = hdr.TimeStamp.QuadPart;
    }

    if (pEvent->PresentMode == PresentMode::Hardware_Legacy_Flip) {
        CompletePresent(pEvent);
    }
}

void PMTraceConsumer::HandleDxgkSyncDPC(EVENT_HEADER const& hdr, uint32_t flipSubmitSequence)
{
    // The VSyncDPC/HSyncDPC contains a field telling us what flipped to screen.
    // This is the way to track completion of a fullscreen present.
    auto eventIter = mPresentBySubmitSequence.find(flipSubmitSequence);
    if (eventIter == mPresentBySubmitSequence.end()) {
        return;
    }
    auto pEvent = eventIter->second;

    TRACK_PRESENT_PATH_SAVE_GENERATED_ID(pEvent);

    DebugModifyPresent(pEvent.get());
    pEvent->ScreenTime = hdr.TimeStamp.QuadPart;
    pEvent->FinalState = PresentResult::Presented;
    if (pEvent->PresentMode == PresentMode::Hardware_Legacy_Flip) {
        CompletePresent(pEvent);
    }
}

void PMTraceConsumer::HandleDxgkSyncDPCMPO(EVENT_HEADER const& hdr, uint32_t flipSubmitSequence, bool isMultiPlane)
{
    // The VSyncDPC/HSyncDPC contains a field telling us what flipped to screen.
    // This is the way to track completion of a fullscreen present.
    auto eventIter = mPresentBySubmitSequence.find(flipSubmitSequence);
    if (eventIter == mPresentBySubmitSequence.end()) {
        return;
    }
    auto pEvent = eventIter->second;

    TRACK_PRESENT_PATH_SAVE_GENERATED_ID(pEvent);

    if (isMultiPlane &&
        (pEvent->PresentMode == PresentMode::Hardware_Independent_Flip || pEvent->PresentMode == PresentMode::Composed_Flip)) {
        DebugModifyPresent(pEvent.get());
        pEvent->PresentMode = PresentMode::Hardware_Composed_Independent_Flip;
    }

    // VSyncDPC and VSyncDPCMultiPlaneOverlay are both sent, with VSyncDPC only
    // including flipSubmitSequence for one layer.  VSyncDPCMultiPlaneOverlay
    // is sent afterward and contains info on whether this vsync/hsync contains
    // an overlay.  So we should avoid updating ScreenTime and FinalState with
    // the second event, but update isMultiPlane with the correct information
    // when we have them.
    if (pEvent->FinalState != PresentResult::Presented) {
        DebugModifyPresent(pEvent.get());
        pEvent->ScreenTime = hdr.TimeStamp.QuadPart;
        pEvent->FinalState = PresentResult::Presented;
    }

    CompletePresent(pEvent);
}

void PMTraceConsumer::HandleDxgkPresentHistory(
    EVENT_HEADER const& hdr,
    uint64_t token,
    uint64_t tokenData,
    Microsoft_Windows_DxgKrnl::PresentModel presentModel)
{
    // These events are emitted during submission of all types of windowed presents while DWM is on.
    // It gives us up to two different types of keys to correlate further.

    // Lookup the in-progress present.  It should not have a known DxgkPresentHistoryToken
    // yet, so DxgkPresentHistoryToken!=0 implies we looked up a 'stuck' present whose
    // tracking was lost for some reason.
    auto presentEvent = FindOrCreatePresent(hdr);
    if (presentEvent == nullptr) {
        return;
    }

    if (presentEvent->DxgkPresentHistoryToken != 0) {
        RemoveLostPresent(presentEvent);
        presentEvent = FindOrCreatePresent(hdr);
        if (presentEvent == nullptr) {
            return;
        }

        DebugAssert(presentEvent->DxgkPresentHistoryToken == 0);
    }

    TRACK_PRESENT_PATH_SAVE_GENERATED_ID(presentEvent);

    DebugModifyPresent(presentEvent.get());
    presentEvent->ReadyTime = 0;
    presentEvent->ScreenTime = 0;
    presentEvent->SupportsTearing = false;
    presentEvent->FinalState = PresentResult::Unknown;
    presentEvent->DxgkPresentHistoryToken = token;

    auto iter = mPresentByDxgkPresentHistoryToken.find(token);
    if (iter != mPresentByDxgkPresentHistoryToken.end()) {
        RemoveLostPresent(iter->second);
    }
    DebugAssert(mPresentByDxgkPresentHistoryToken.find(token) == mPresentByDxgkPresentHistoryToken.end());
    mPresentByDxgkPresentHistoryToken[token] = presentEvent;

    if (presentEvent->PresentMode == PresentMode::Hardware_Legacy_Copy_To_Front_Buffer) {
        DebugAssert(presentModel == Microsoft_Windows_DxgKrnl::PresentModel::D3DKMT_PM_UNINITIALIZED ||
                    presentModel == Microsoft_Windows_DxgKrnl::PresentModel::D3DKMT_PM_REDIRECTED_BLT);
        presentEvent->PresentMode = PresentMode::Composed_Copy_GPU_GDI;

    } else if (presentEvent->PresentMode == PresentMode::Unknown) {
        if (presentModel == Microsoft_Windows_DxgKrnl::PresentModel::D3DKMT_PM_REDIRECTED_COMPOSITION) {
            /* BEGIN WORKAROUND: DirectComposition presents are currently ignored. There
             * were rare situations observed where they would lead to issues during present
             * completion (including stackoverflow caused by recursive completion).  This
             * could not be diagnosed, so we removed support for this present mode for now...
            presentEvent->PresentMode = PresentMode::Composed_Composition_Atlas;
            */
            RemoveLostPresent(presentEvent);
            return;
            /* END WORKAROUND */
        } else {
            // When there's no Win32K events, we'll assume PHTs that aren't after a blt, and aren't composition tokens
            // are flip tokens and that they're displayed. There are no Win32K events on Win7, and they might not be
            // present in some traces - don't let presents get stuck/dropped just because we can't track them perfectly.
            DebugAssert(!presentEvent->SeenWin32KEvents);
            presentEvent->PresentMode = PresentMode::Composed_Flip;
        }
    } else if (presentEvent->PresentMode == PresentMode::Composed_Copy_CPU_GDI) {
        if (tokenData == 0) {
            // This is the best we can do, we won't be able to tell how many frames are actually displayed.
            mPresentsWaitingForDWM.emplace_back(presentEvent);
            presentEvent->PresentInDwmWaitingStruct = true;
        } else {
            DebugAssert(mPresentByDxgkPresentHistoryTokenData.find(tokenData) == mPresentByDxgkPresentHistoryTokenData.end());
            mPresentByDxgkPresentHistoryTokenData[tokenData] = presentEvent;
            presentEvent->DxgkPresentHistoryTokenData = tokenData;
        }
    }

    // If we are not tracking further GPU/display-related events, complete the
    // present here.
    if (!mTrackDisplay) {
        CompletePresent(presentEvent);
    }
}

void PMTraceConsumer::HandleDxgkPresentHistoryInfo(EVENT_HEADER const& hdr, uint64_t token)
{
    // This event is emitted when a token is being handed off to DWM, and is a good way to indicate a ready state
    auto eventIter = mPresentByDxgkPresentHistoryToken.find(token);
    if (eventIter == mPresentByDxgkPresentHistoryToken.end()) {
        return;
    }

    DebugModifyPresent(eventIter->second.get());
    TRACK_PRESENT_PATH_SAVE_GENERATED_ID(eventIter->second);

    eventIter->second->ReadyTime = eventIter->second->ReadyTime == 0
        ? hdr.TimeStamp.QuadPart
        : std::min(eventIter->second->ReadyTime, (uint64_t) hdr.TimeStamp.QuadPart);

    // Neither Composed Composition Atlas or Win7 Flip has DWM events indicating intent
    // to present this frame.
    //
    // Composed presents are currently ignored.
    if (//eventIter->second->PresentMode == PresentMode::Composed_Composition_Atlas ||
        (eventIter->second->PresentMode == PresentMode::Composed_Flip && !eventIter->second->SeenWin32KEvents)) {
        mPresentsWaitingForDWM.emplace_back(eventIter->second);
        eventIter->second->PresentInDwmWaitingStruct = true;
        eventIter->second->DwmNotified = true;
    }

    if (eventIter->second->PresentMode == PresentMode::Composed_Copy_GPU_GDI) {
        // This present will be handed off to DWM.  When DWM is ready to
        // present, we'll query for the most recent blt targeting this window
        // and take it out of the map.
        mLastPresentByWindow[eventIter->second->Hwnd] = eventIter->second;
    }

    mPresentByDxgkPresentHistoryToken.erase(eventIter);
}

void PMTraceConsumer::HandleDXGKEvent(EVENT_RECORD* pEventRecord)
{
    DebugEvent(pEventRecord, &mMetadata);

    auto const& hdr = pEventRecord->EventHeader;
    switch (hdr.EventDescriptor.Id) {
    case Microsoft_Windows_DxgKrnl::Flip_Info::Id:
    {
        EventDataDesc desc[] = {
            { L"FlipInterval" },
            { L"MMIOFlip" },
        };
        mMetadata.GetEventData(pEventRecord, desc, _countof(desc));
        auto FlipInterval = desc[0].GetData<uint32_t>();
        auto MMIOFlip     = desc[1].GetData<BOOL>() != 0;

        TRACK_PRESENT_PATH_GENERATE_ID();
        HandleDxgkFlip(hdr, FlipInterval, MMIOFlip);
        return;
    }
    case Microsoft_Windows_DxgKrnl::IndependentFlip_Info::Id:
    {
        EventDataDesc desc[] = {
            { L"SubmitSequence" },
            { L"FlipInterval" },
        };
        mMetadata.GetEventData(pEventRecord, desc, _countof(desc));
        auto SubmitSequence = desc[0].GetData<uint32_t>();
        auto FlipInterval   = desc[1].GetData<uint32_t>();

        auto eventIter = mPresentBySubmitSequence.find(SubmitSequence);
        if (eventIter != mPresentBySubmitSequence.end()) {
            auto pEvent = eventIter->second;

            // We should not have already identified as hardware_composed - this
            // can only be detected around Vsync/HsyncDPC time.
            DebugAssert(pEvent->PresentMode != PresentMode::Hardware_Composed_Independent_Flip);

            DebugModifyPresent(pEvent.get());
            pEvent->PresentMode = PresentMode::Hardware_Independent_Flip;
            pEvent->SyncInterval = FlipInterval;
        }
        return;
    }
    case Microsoft_Windows_DxgKrnl::FlipMultiPlaneOverlay_Info::Id:
        TRACK_PRESENT_PATH_GENERATE_ID();
        HandleDxgkFlip(hdr, -1, true);
        return;
    case Microsoft_Windows_DxgKrnl::QueuePacket_Start::Id:
    {
        EventDataDesc desc[] = {
            { L"PacketType" },
            { L"SubmitSequence" },
            { L"hContext" },
            { L"bPresent" },
        };
        mMetadata.GetEventData(pEventRecord, desc, _countof(desc));
        auto PacketType     = desc[0].GetData<uint32_t>();
        auto SubmitSequence = desc[1].GetData<uint32_t>();
        auto hContext       = desc[2].GetData<uint64_t>();
        auto bPresent       = desc[3].GetData<BOOL>() != 0;

        HandleDxgkQueueSubmit<false>(hdr, hContext, SubmitSequence, PacketType, bPresent);
        return;
    }
    case Microsoft_Windows_DxgKrnl::QueuePacket_Stop::Id:
        TRACK_PRESENT_PATH_GENERATE_ID();
        HandleDxgkQueueComplete(hdr, mMetadata.GetEventData<uint32_t>(pEventRecord, L"SubmitSequence"));
        return;
    case Microsoft_Windows_DxgKrnl::MMIOFlip_Info::Id:
    {
        EventDataDesc desc[] = {
            { L"FlipSubmitSequence" },
            { L"Flags" },
        };
        mMetadata.GetEventData(pEventRecord, desc, _countof(desc));
        auto FlipSubmitSequence = desc[0].GetData<uint32_t>();
        auto Flags              = desc[1].GetData<uint32_t>();

        TRACK_PRESENT_PATH_GENERATE_ID();
        HandleDxgkMMIOFlip(hdr, FlipSubmitSequence, Flags);
        return;
    }
    case Microsoft_Windows_DxgKrnl::MMIOFlipMultiPlaneOverlay_Info::Id:
    {
        auto flipEntryStatusAfterFlipValid = hdr.EventDescriptor.Version >= 2;
        EventDataDesc desc[] = {
            { L"FlipSubmitSequence" },
            { L"FlipEntryStatusAfterFlip" }, // optional
        };
        mMetadata.GetEventData(pEventRecord, desc, _countof(desc) - (flipEntryStatusAfterFlipValid ? 0 : 1));
        auto FlipFenceId              = desc[0].GetData<uint64_t>();
        auto FlipEntryStatusAfterFlip = flipEntryStatusAfterFlipValid ? desc[1].GetData<uint32_t>() : 0u;

        auto flipSubmitSequence = (uint32_t) (FlipFenceId >> 32u);

        HandleDxgkMMIOFlipMPO(hdr, flipSubmitSequence, FlipEntryStatusAfterFlip, flipEntryStatusAfterFlipValid);
        return;
    }
    case Microsoft_Windows_DxgKrnl::VSyncDPCMultiPlane_Info::Id:
    case Microsoft_Windows_DxgKrnl::HSyncDPCMultiPlane_Info::Id:
    {
        // HSync is used for Hardware Independent Flip, and Hardware Composed
        // Flip to signal flipping to the screen on Windows 10 build numbers
        // 17134 and up where the associated display is connected to integrated
        // graphics
        //
        // MMIOFlipMPO [EntryStatus:FlipWaitHSync] -> HSync DPC

        TRACK_PRESENT_PATH_GENERATE_ID();

        EventDataDesc desc[] = {
            { L"PlaneCount" },
            { L"FlipEntryCount" },
        };
        mMetadata.GetEventData(pEventRecord, desc, _countof(desc));
        auto PlaneCount = desc[0].GetData<uint32_t>();
        auto FlipCount  = desc[1].GetData<uint32_t>();

        if (FlipCount > 0) {
            // The number of active planes is determined by the number of non-zero
            // PresentIdOrPhysicalAddress (VSync) or ScannedPhysicalAddress (HSync)
            // properties.
            auto addressPropName = (hdr.EventDescriptor.Id == Microsoft_Windows_DxgKrnl::VSyncDPCMultiPlane_Info::Id && hdr.EventDescriptor.Version >= 1)
                ? L"PresentIdOrPhysicalAddress"
                : L"ScannedPhysicalAddress";

            uint32_t activePlaneCount = 0;
            for (uint32_t id = 0; id < PlaneCount; id++) {
                if (mMetadata.GetEventData<uint64_t>(pEventRecord, addressPropName, id) != 0) {
                    activePlaneCount++;
                }
            }

            auto isMultiPlane = activePlaneCount > 1;
            for (uint32_t i = 0; i < FlipCount; i++) {
                auto FlipId = mMetadata.GetEventData<uint64_t>(pEventRecord, L"FlipSubmitSequence", i);
                HandleDxgkSyncDPCMPO(hdr, (uint32_t)(FlipId >> 32u), isMultiPlane);
            }
        }
        return;
    }
    case Microsoft_Windows_DxgKrnl::VSyncDPC_Info::Id:
    {
        TRACK_PRESENT_PATH_GENERATE_ID();

        auto FlipFenceId = mMetadata.GetEventData<uint64_t>(pEventRecord, L"FlipFenceId");
        HandleDxgkSyncDPC(hdr, (uint32_t)(FlipFenceId >> 32u));
        return;
    }
    case Microsoft_Windows_DxgKrnl::Present_Info::Id:
    {
        // This event is emitted at the end of the kernel present, before returning.
        auto eventIter = mPresentByThreadId.find(hdr.ThreadId);
        if (eventIter != mPresentByThreadId.end()) {
            auto present = eventIter->second;

            TRACK_PRESENT_PATH(present);

            // Store the fact we've seen this present.  This is used to improve
            // tracking and to defer blt present completion until both Present_Info
            // and present QueuePacket_Stop have been seen.
            DebugModifyPresent(present.get());
            present->SeenDxgkPresent = true;

            if (present->Hwnd == 0) {
                present->Hwnd = mMetadata.GetEventData<uint64_t>(pEventRecord, L"hWindow");
            }

            // If we are not expecting an API present end event, then this
            // should be the last operation on this thread.  This can happen
            // due to batched presents or non-instrumented present APIs (i.e.,
            // not DXGI nor D3D9).
            if (present->Runtime == Runtime::Other ||
                present->ThreadId != hdr.ThreadId) {
                mPresentByThreadId.erase(eventIter);
            }

            // If this is a deferred blit that's already seen QueuePacket_Stop,
            // then complete it now.
            if (present->PresentMode == PresentMode::Hardware_Legacy_Copy_To_Front_Buffer &&
                present->ScreenTime != 0) {
                CompletePresent(present);
            }
        }
        return;
    }
    case Microsoft_Windows_DxgKrnl::PresentHistoryDetailed_Start::Id:
    case Microsoft_Windows_DxgKrnl::PresentHistory_Start::Id:
    {
        EventDataDesc desc[] = {
            { L"Token" },
            { L"Model" },
            { L"TokenData" },
        };
        mMetadata.GetEventData(pEventRecord, desc, _countof(desc));
        auto Token     = desc[0].GetData<uint64_t>();
        auto Model     = desc[1].GetData<Microsoft_Windows_DxgKrnl::PresentModel>();
        auto TokenData = desc[2].GetData<uint64_t>();

        if (Model != Microsoft_Windows_DxgKrnl::PresentModel::D3DKMT_PM_REDIRECTED_GDI) {
            TRACK_PRESENT_PATH_GENERATE_ID();
            HandleDxgkPresentHistory(hdr, Token, TokenData, Model);
        }
        return;
    }
    case Microsoft_Windows_DxgKrnl::PresentHistory_Info::Id:
        TRACK_PRESENT_PATH_GENERATE_ID();
        HandleDxgkPresentHistoryInfo(hdr, mMetadata.GetEventData<uint64_t>(pEventRecord, L"Token"));
        return;
    case Microsoft_Windows_DxgKrnl::Blit_Info::Id:
    {
        EventDataDesc desc[] = {
            { L"hwnd" },
            { L"bRedirectedPresent" },
        };
        mMetadata.GetEventData(pEventRecord, desc, _countof(desc));
        auto hwnd               = desc[0].GetData<uint64_t>();
        auto bRedirectedPresent = desc[1].GetData<uint32_t>() != 0;

        TRACK_PRESENT_PATH_GENERATE_ID();
        HandleDxgkBlt(hdr, hwnd, bRedirectedPresent);
        return;
    }
    case Microsoft_Windows_DxgKrnl::BlitCancel_Info::Id:
        HandleDxgkBltCancel(hdr);
        return;
    }

    if (mTrackGPU || mTrackINTCTimers) {
        switch (hdr.EventDescriptor.Id) {

        // We need a mapping from hContext to GPU node.
        //
        // There's two ways I've tried to get this. One is to use
        // Microsoft_Windows_DxgKrnl::SelectContext2_Info events which include
        // all the required info (hContext, pDxgAdapter, and NodeOrdinal) but
        // that event fires often leading to significant overhead.
        //
        // The current implementaiton requires a CAPTURE_STATE on start up to
        // get all existing context/device events but after that the event
        // overhead should be minimal.
        case Microsoft_Windows_DxgKrnl::Device_DCStart::Id:
        case Microsoft_Windows_DxgKrnl::Device_Start::Id:
        {
            EventDataDesc desc[] = {
                { L"pDxgAdapter" },
                { L"hDevice" },
            };
            mMetadata.GetEventData(pEventRecord, desc, _countof(desc));
            auto pDxgAdapter = desc[0].GetData<uint64_t>();
            auto hDevice     = desc[1].GetData<uint64_t>();

            mGpuTrace.RegisterDevice(hDevice, pDxgAdapter);
            return;
        }
        // Sometimes a trace will miss a Device_Start, so we also check
        // AdapterAllocation events (which also provide the pDxgAdapter-hDevice
        // mapping).  These are not currently enabled for realtime collection.
        case Microsoft_Windows_DxgKrnl::AdapterAllocation_Start::Id:
        case Microsoft_Windows_DxgKrnl::AdapterAllocation_DCStart::Id:
        case Microsoft_Windows_DxgKrnl::AdapterAllocation_Stop::Id:
        {
            EventDataDesc desc[] = {
                { L"pDxgAdapter" },
                { L"hDevice" },
            };
            mMetadata.GetEventData(pEventRecord, desc, _countof(desc));
            auto pDxgAdapter = desc[0].GetData<uint64_t>();
            auto hDevice     = desc[1].GetData<uint64_t>();

            if (hDevice != 0) {
                mGpuTrace.RegisterDevice(hDevice, pDxgAdapter);
            }
            return;
        }
        case Microsoft_Windows_DxgKrnl::Device_Stop::Id:
        {
            auto hDevice = mMetadata.GetEventData<uint64_t>(pEventRecord, L"hDevice");

            mGpuTrace.UnregisterDevice(hDevice);
            return;
        }
        case Microsoft_Windows_DxgKrnl::Context_DCStart::Id:
        case Microsoft_Windows_DxgKrnl::Context_Start::Id:
        {
            EventDataDesc desc[] = {
                { L"hContext" },
                { L"hDevice" },
                { L"NodeOrdinal" },
            };
            mMetadata.GetEventData(pEventRecord, desc, _countof(desc));
            auto hContext    = desc[0].GetData<uint64_t>();
            auto hDevice     = desc[1].GetData<uint64_t>();
            auto NodeOrdinal = desc[2].GetData<uint32_t>();

            // If this is a DCStart, then it was generated by xperf instead of
            // the context's process.
            uint32_t processId = hdr.EventDescriptor.Id == Microsoft_Windows_DxgKrnl::Context_DCStart::Id
                ? 0
                : hdr.ProcessId;

            mGpuTrace.RegisterContext(hContext, hDevice, NodeOrdinal, processId);
            return;
        }
        case Microsoft_Windows_DxgKrnl::Context_Stop::Id:
        {
            auto hContext = mMetadata.GetEventData<uint64_t>(pEventRecord, L"hContext");

            mGpuTrace.UnregisterContext(hContext);
            return;
        }

        case Microsoft_Windows_DxgKrnl::NodeMetadata_Info::Id:
        {
            EventDataDesc desc[] = {
                { L"pDxgAdapter" },
                { L"NodeOrdinal" },
                { L"EngineType" },
            };
            mMetadata.GetEventData(pEventRecord, desc, _countof(desc));
            auto pDxgAdapter = desc[0].GetData<uint64_t>();
            auto NodeOrdinal = desc[1].GetData<uint32_t>();
            auto EngineType  = desc[2].GetData<Microsoft_Windows_DxgKrnl::DXGK_ENGINE>();

            mGpuTrace.SetEngineType(pDxgAdapter, NodeOrdinal, EngineType);
            return;
        }

        // DmaPacket_Start occurs when a packet is enqueued onto a node.
        // 
        // There are certain DMA packets that don't result in GPU work.
        // Examples are preemption packets or notifications for
        // VIDSCH_QUANTUM_EXPIRED.  These will have a sequence id of zero (also
        // DmaBuffer will be null).
        case Microsoft_Windows_DxgKrnl::DmaPacket_Start::Id:
        {
            EventDataDesc desc[] = {
                { L"hContext" },
                { L"ulQueueSubmitSequence" },
            };
            mMetadata.GetEventData(pEventRecord, desc, _countof(desc));
            auto hContext   = desc[0].GetData<uint64_t>();
            auto SequenceId = desc[1].GetData<uint32_t>();

            if (SequenceId != 0) {
                mGpuTrace.EnqueueDmaPacket(hContext, SequenceId, hdr.TimeStamp.QuadPart);
            }
            return;
        }

        // DmaPacket_Info occurs on packet-related interrupts.  We could use
        // DmaPacket_Stop here, but the DMA_COMPLETED interrupt is a tighter
        // bound.
        case Microsoft_Windows_DxgKrnl::DmaPacket_Info::Id:
        {
            EventDataDesc desc[] = {
                { L"hContext" },
                { L"ulQueueSubmitSequence" },
            };
            mMetadata.GetEventData(pEventRecord, desc, _countof(desc));
            auto hContext   = desc[0].GetData<uint64_t>();
            auto SequenceId = desc[1].GetData<uint32_t>();

            if (SequenceId != 0) {
                auto cloudStreamingProcessId = mGpuTrace.CompleteDmaPacket(hContext, SequenceId, hdr.TimeStamp.QuadPart);

                // If this is the end of an identified video encode packet on a context
                // used for cloud streaming, treat it like a present.
                if (cloudStreamingProcessId != 0) {
                    auto videoPresent = std::make_shared<PresentEvent>(hdr, Runtime::CloudStreaming);
                    videoPresent->ProcessId = cloudStreamingProcessId;
                    videoPresent->IsCompleted = true;

                    mGpuTrace.CompleteFrame(videoPresent.get(), hdr.TimeStamp.QuadPart);

                    {
                        std::lock_guard<std::mutex> lock(mPresentEventMutex);
                        mCompletePresentEvents.push_back(videoPresent);
                    }
                }
            }
            return;
        }
        }
    }

    if (mTrackMemoryResidency) {
        switch (hdr.EventDescriptor.Id) {

        // Memory residency tracking.
        case Microsoft_Windows_DxgKrnl::MakeResident_Start::Id:
            mGpuTrace.StartMakeResident(hdr.ProcessId, hdr.TimeStamp.QuadPart);
            return;
        case Microsoft_Windows_DxgKrnl::MakeResident_Stop::Id:
            mGpuTrace.StopMakeResident(hdr.ProcessId, hdr.TimeStamp.QuadPart);
            return;

        // Paging queue packet tracking.
        //
        // PagingQueuePacket_Start is called from the target process, and we
        // use it to track the association of process id to paging packet
        // sequence id.
        //
        // PagingQueuePacket_Info indicates the start of packet processing on
        // the CPU.
        //
        // PagingQueuePacket_Stop indicates the end of packet processing on the
        // CPU.
        case Microsoft_Windows_DxgKrnl::PagingQueuePacket_Start::Id:
            mGpuTrace.RegisterPagingQueuePacket(hdr.ProcessId, mMetadata.GetEventData<uint64_t>(pEventRecord, L"SequenceId"));
            return;
        case Microsoft_Windows_DxgKrnl::PagingQueuePacket_Info::Id:
            mGpuTrace.StartPagingQueuePacket(mMetadata.GetEventData<uint64_t>(pEventRecord, L"SequenceId"), hdr.TimeStamp.QuadPart);
            return;
        case Microsoft_Windows_DxgKrnl::PagingQueuePacket_Stop::Id:
            mGpuTrace.StopPagingQueuePacket(mMetadata.GetEventData<uint64_t>(pEventRecord, L"SequenceId"), hdr.TimeStamp.QuadPart);
            return;
        }
    }

    assert(!mFilteredEvents); // Assert that filtering is working if expected
}

void PMTraceConsumer::HandleWin7DxgkBlt(EVENT_RECORD* pEventRecord)
{
    using namespace Microsoft_Windows_DxgKrnl::Win7;

    DebugEvent(pEventRecord, &mMetadata);
    TRACK_PRESENT_PATH_GENERATE_ID();

    auto pBltEvent = reinterpret_cast<DXGKETW_BLTEVENT*>(pEventRecord->UserData);
    HandleDxgkBlt(
        pEventRecord->EventHeader,
        pBltEvent->hwnd,
        pBltEvent->bRedirectedPresent != 0);
}

void PMTraceConsumer::HandleWin7DxgkFlip(EVENT_RECORD* pEventRecord)
{
    using namespace Microsoft_Windows_DxgKrnl::Win7;

    DebugEvent(pEventRecord, &mMetadata);
    TRACK_PRESENT_PATH_GENERATE_ID();

    auto pFlipEvent = reinterpret_cast<DXGKETW_FLIPEVENT*>(pEventRecord->UserData);
    HandleDxgkFlip(
        pEventRecord->EventHeader,
        pFlipEvent->FlipInterval,
        pFlipEvent->MMIOFlip != 0);
}

void PMTraceConsumer::HandleWin7DxgkPresentHistory(EVENT_RECORD* pEventRecord)
{
    using namespace Microsoft_Windows_DxgKrnl::Win7;

    DebugEvent(pEventRecord, &mMetadata);

    auto pPresentHistoryEvent = reinterpret_cast<DXGKETW_PRESENTHISTORYEVENT*>(pEventRecord->UserData);
    if (pEventRecord->EventHeader.EventDescriptor.Opcode == EVENT_TRACE_TYPE_START) {
        TRACK_PRESENT_PATH_GENERATE_ID();
        HandleDxgkPresentHistory(
            pEventRecord->EventHeader,
            pPresentHistoryEvent->Token,
            0,
            Microsoft_Windows_DxgKrnl::PresentModel::D3DKMT_PM_UNINITIALIZED);
    } else if (pEventRecord->EventHeader.EventDescriptor.Opcode == EVENT_TRACE_TYPE_INFO) {
        TRACK_PRESENT_PATH_GENERATE_ID();
        HandleDxgkPresentHistoryInfo(pEventRecord->EventHeader, pPresentHistoryEvent->Token);
    }
}

void PMTraceConsumer::HandleWin7DxgkQueuePacket(EVENT_RECORD* pEventRecord)
{
    using namespace Microsoft_Windows_DxgKrnl::Win7;

    DebugEvent(pEventRecord, &mMetadata);

    if (pEventRecord->EventHeader.EventDescriptor.Opcode == EVENT_TRACE_TYPE_START) {
        auto pSubmitEvent = reinterpret_cast<DXGKETW_QUEUESUBMITEVENT*>(pEventRecord->UserData);
        HandleDxgkQueueSubmit<true>(
            pEventRecord->EventHeader,
            pSubmitEvent->hContext,
            pSubmitEvent->SubmitSequence,
            pSubmitEvent->PacketType,
            pSubmitEvent->bPresent != 0);
    } else if (pEventRecord->EventHeader.EventDescriptor.Opcode == EVENT_TRACE_TYPE_STOP) {
        auto pCompleteEvent = reinterpret_cast<DXGKETW_QUEUECOMPLETEEVENT*>(pEventRecord->UserData);
        TRACK_PRESENT_PATH_GENERATE_ID();
        HandleDxgkQueueComplete(pEventRecord->EventHeader, pCompleteEvent->SubmitSequence);
    }
}

void PMTraceConsumer::HandleWin7DxgkVSyncDPC(EVENT_RECORD* pEventRecord)
{
    using namespace Microsoft_Windows_DxgKrnl::Win7;

    DebugEvent(pEventRecord, &mMetadata);
    TRACK_PRESENT_PATH_GENERATE_ID();

    auto pVSyncDPCEvent = reinterpret_cast<DXGKETW_SCHEDULER_VSYNC_DPC*>(pEventRecord->UserData);

    // Windows 7 does not support MultiPlaneOverlay.
    HandleDxgkSyncDPC(pEventRecord->EventHeader, (uint32_t)(pVSyncDPCEvent->FlipFenceId.QuadPart >> 32u));
}

void PMTraceConsumer::HandleWin7DxgkMMIOFlip(EVENT_RECORD* pEventRecord)
{
    using namespace Microsoft_Windows_DxgKrnl::Win7;

    DebugEvent(pEventRecord, &mMetadata);
    TRACK_PRESENT_PATH_GENERATE_ID();

    if (pEventRecord->EventHeader.Flags & EVENT_HEADER_FLAG_32_BIT_HEADER)
    {
        auto pMMIOFlipEvent = reinterpret_cast<DXGKETW_SCHEDULER_MMIO_FLIP_32*>(pEventRecord->UserData);
        HandleDxgkMMIOFlip(
            pEventRecord->EventHeader,
            pMMIOFlipEvent->FlipSubmitSequence,
            pMMIOFlipEvent->Flags);
    }
    else
    {
        auto pMMIOFlipEvent = reinterpret_cast<DXGKETW_SCHEDULER_MMIO_FLIP_64*>(pEventRecord->UserData);
        HandleDxgkMMIOFlip(
            pEventRecord->EventHeader,
            pMMIOFlipEvent->FlipSubmitSequence,
            pMMIOFlipEvent->Flags);
    }
}

std::size_t PMTraceConsumer::Win32KPresentHistoryTokenHash::operator()(PMTraceConsumer::Win32KPresentHistoryToken const& v) const noexcept
{
    auto CompositionSurfaceLuid = std::get<0>(v);
    auto PresentCount           = std::get<1>(v);
    auto BindId                 = std::get<2>(v);
    PresentCount = (PresentCount << 32) | (PresentCount >> (64-32));
    BindId       = (BindId       << 56) | (BindId       >> (64-56));
    auto h64 = CompositionSurfaceLuid ^ PresentCount ^ BindId;
    return std::hash<uint64_t>::operator()(h64);
}

void PMTraceConsumer::HandleWin32kEvent(EVENT_RECORD* pEventRecord)
{
    DebugEvent(pEventRecord, &mMetadata);

    auto const& hdr = pEventRecord->EventHeader;
    switch (hdr.EventDescriptor.Id) {
    case Microsoft_Windows_Win32k::TokenCompositionSurfaceObject_Info::Id:
    {
        EventDataDesc desc[] = {
            { L"CompositionSurfaceLuid" },
            { L"PresentCount" },
            { L"BindId" },
            { L"DestWidth" },  // version >= 1
            { L"DestHeight" }, // version >= 1
        };
        mMetadata.GetEventData(pEventRecord, desc, _countof(desc) - (hdr.EventDescriptor.Version == 0 ? 2 : 0));
        auto CompositionSurfaceLuid = desc[0].GetData<uint64_t>();
        auto PresentCount           = desc[1].GetData<uint64_t>();
        auto BindId                 = desc[2].GetData<uint64_t>();

        // Lookup the in-progress present.  It should not have seen any Win32K
        // events yet, so SeenWin32KEvents==true implies we looked up a 'stuck'
        // present whose tracking was lost for some reason.
        auto PresentEvent = FindOrCreatePresent(hdr);
        if (PresentEvent == nullptr) {
            return;
        }

        if (PresentEvent->SeenWin32KEvents) {
            RemoveLostPresent(PresentEvent);
            PresentEvent = FindOrCreatePresent(hdr);
            if (PresentEvent == nullptr) {
                return;
            }

            DebugAssert(!PresentEvent->SeenWin32KEvents);
        }

        TRACK_PRESENT_PATH(PresentEvent);

        PresentEvent->PresentMode = PresentMode::Composed_Flip;
        PresentEvent->SeenWin32KEvents = true;

        if (hdr.EventDescriptor.Version >= 1) {
            PresentEvent->DestWidth  = desc[3].GetData<uint32_t>();
            PresentEvent->DestHeight = desc[4].GetData<uint32_t>();
        }

        Win32KPresentHistoryToken key(CompositionSurfaceLuid, PresentCount, BindId);
        DebugAssert(mPresentByWin32KPresentHistoryToken.find(key) == mPresentByWin32KPresentHistoryToken.end());
        mPresentByWin32KPresentHistoryToken[key] = PresentEvent;
        PresentEvent->CompositionSurfaceLuid = CompositionSurfaceLuid;
        PresentEvent->Win32KPresentCount = PresentCount;
        PresentEvent->Win32KBindId = BindId;
        break;
    }

    case Microsoft_Windows_Win32k::TokenStateChanged_Info::Id:
    {
        EventDataDesc desc[] = {
            { L"CompositionSurfaceLuid" },
            { L"PresentCount" },
            { L"BindId" },
            { L"NewState" },
        };
        mMetadata.GetEventData(pEventRecord, desc, _countof(desc));
        auto CompositionSurfaceLuid = desc[0].GetData<uint64_t>();
        auto PresentCount           = desc[1].GetData<uint32_t>();
        auto BindId                 = desc[2].GetData<uint64_t>();
        auto NewState               = desc[3].GetData<uint32_t>();

        Win32KPresentHistoryToken key(CompositionSurfaceLuid, PresentCount, BindId);
        auto eventIter = mPresentByWin32KPresentHistoryToken.find(key);
        if (eventIter == mPresentByWin32KPresentHistoryToken.end()) {
            return;
        }
        auto presentEvent = eventIter->second;

        switch (NewState) {
        case (uint32_t) Microsoft_Windows_Win32k::TokenState::InFrame: // Composition is starting
        {
            TRACK_PRESENT_PATH(presentEvent);

            // We won't necessarily see a transition to Discarded for all
            // presents. If we're compositing a newer present than the window's
            // last known present, then the last known one was discarded.
            if (presentEvent->Hwnd) {
                auto hWndIter = mLastPresentByWindow.find(presentEvent->Hwnd);
                if (hWndIter == mLastPresentByWindow.end()) {
                    mLastPresentByWindow.emplace(presentEvent->Hwnd, presentEvent);
                } else if (hWndIter->second != presentEvent) {
                    DebugModifyPresent(hWndIter->second.get());
                    hWndIter->second->FinalState = PresentResult::Discarded;    // TODO: Complete the present here?
                    hWndIter->second = presentEvent;
                }
            }

            DebugModifyPresent(presentEvent.get());
            presentEvent->SeenInFrameEvent = true;

            bool iFlip = mMetadata.GetEventData<BOOL>(pEventRecord, L"IndependentFlip") != 0;
            if (iFlip && presentEvent->PresentMode == PresentMode::Composed_Flip) {
                presentEvent->PresentMode = PresentMode::Hardware_Independent_Flip;
            }
            break;
        }

        case (uint32_t) Microsoft_Windows_Win32k::TokenState::Confirmed: // Present has been submitted
            TRACK_PRESENT_PATH(presentEvent);

            // Handle DO_NOT_SEQUENCE presents, which may get marked as confirmed,
            // if a frame was composed when this token was completed
            if (presentEvent->FinalState == PresentResult::Unknown &&
                (presentEvent->PresentFlags & DXGI_PRESENT_DO_NOT_SEQUENCE) != 0) {
                DebugModifyPresent(presentEvent.get());
                presentEvent->FinalState = PresentResult::Discarded;
            }
            if (presentEvent->Hwnd) {
                mLastPresentByWindow.erase(presentEvent->Hwnd);
            }
            break;

        // Note: Going forward, TokenState::Retired events are no longer
        // guaranteed to be sent at the end of a frame in multi-monitor
        // scenarios.  Instead, we use DWM's present stats to understand the
        // Composed Flip timeline.
        case (uint32_t) Microsoft_Windows_Win32k::TokenState::Discarded: // Present has been discarded
        {
            TRACK_PRESENT_PATH(presentEvent);

            mPresentByWin32KPresentHistoryToken.erase(eventIter);

            if (!presentEvent->SeenInFrameEvent && (presentEvent->FinalState == PresentResult::Unknown || presentEvent->ScreenTime == 0)) {
                DebugModifyPresent(presentEvent.get());
                presentEvent->FinalState = PresentResult::Discarded;
                CompletePresent(presentEvent);
            } else if (presentEvent->PresentMode != PresentMode::Composed_Flip) {
                CompletePresent(presentEvent);
            }

            break;
        }
        }
        break;
    }

    case Microsoft_Windows_Win32k::InputDeviceRead_Stop::Id:
    {
        EventDataDesc desc[] = {
            { L"DeviceType" },
        };
        mMetadata.GetEventData(pEventRecord, desc, _countof(desc));
        auto DeviceType = desc[0].GetData<uint32_t>();

        switch (DeviceType) {
        case 0: mLastInputDeviceType = InputDeviceType::Mouse; break;
        case 1: mLastInputDeviceType = InputDeviceType::Keyboard; break;
        default: mLastInputDeviceType = InputDeviceType::Unknown; break;
        }

        mLastInputDeviceReadTime = hdr.TimeStamp.QuadPart;
        break;
    }

    case Microsoft_Windows_Win32k::RetrieveInputMessage_Info::Id:
    {
        auto ii = mRetrievedInput.find(hdr.ProcessId);
        if (ii == mRetrievedInput.end()) {
            mRetrievedInput.emplace(hdr.ProcessId, std::make_pair(
                mLastInputDeviceReadTime,
                mLastInputDeviceType));
        } else {
            if (ii->second.first == 0) {
                ii->second.first = mLastInputDeviceReadTime;
                ii->second.second = mLastInputDeviceType;
            }
        }
        break;
    }

    default:
        assert(!mFilteredEvents); // Assert that filtering is working if expected
        break;
    }
}

void PMTraceConsumer::HandleDWMEvent(EVENT_RECORD* pEventRecord)
{
    DebugEvent(pEventRecord, &mMetadata);

    auto const& hdr = pEventRecord->EventHeader;
    switch (hdr.EventDescriptor.Id) {
    case Microsoft_Windows_Dwm_Core::MILEVENT_MEDIA_UCE_PROCESSPRESENTHISTORY_GetPresentHistory_Info::Id:
        // Move all the latest in-progress Composed_Copy from each window into
        // mPresentsWaitingForDWM, to be attached to the next DWM present's
        // DependentPresents.
        for (auto& hWndPair : mLastPresentByWindow) {
            auto& present = hWndPair.second;
            if (present->PresentMode == PresentMode::Composed_Copy_GPU_GDI ||
                present->PresentMode == PresentMode::Composed_Copy_CPU_GDI) {
                TRACK_PRESENT_PATH(present);
                DebugModifyPresent(present.get());
                present->DwmNotified = true;
                mPresentsWaitingForDWM.emplace_back(present);
                present->PresentInDwmWaitingStruct = true;
            }
        }
        mLastPresentByWindow.clear();
        break;

    case Microsoft_Windows_Dwm_Core::SCHEDULE_PRESENT_Start::Id:
        DwmProcessId = hdr.ProcessId;
        DwmPresentThreadId = hdr.ThreadId;
        break;

    // These events are only used for Composed_Copy_CPU_GDI presents.  They are
    // used to identify when such presents are handed off to DWM. 
    case Microsoft_Windows_Dwm_Core::FlipChain_Pending::Id:
    case Microsoft_Windows_Dwm_Core::FlipChain_Complete::Id:
    case Microsoft_Windows_Dwm_Core::FlipChain_Dirty::Id:
    {
        if (InlineIsEqualGUID(hdr.ProviderId, Microsoft_Windows_Dwm_Core::Win7::GUID)) {
            break;
        }

        EventDataDesc desc[] = {
            { L"ulFlipChain" },
            { L"ulSerialNumber" },
            { L"hwnd" },
        };
        mMetadata.GetEventData(pEventRecord, desc, _countof(desc));
        auto ulFlipChain    = desc[0].GetData<uint32_t>();
        auto ulSerialNumber = desc[1].GetData<uint32_t>();
        auto hwnd           = desc[2].GetData<uint64_t>();

        // Lookup the present using the 64-bit token data from the PHT
        // submission, which is actually two 32-bit data chunks corresponding
        // to a flip chain id and present id.
        auto tokenData = ((uint64_t) ulFlipChain << 32ull) | ulSerialNumber;
        auto flipIter = mPresentByDxgkPresentHistoryTokenData.find(tokenData);
        if (flipIter != mPresentByDxgkPresentHistoryTokenData.end()) {
            auto present = flipIter->second;

            TRACK_PRESENT_PATH(present);

            DebugModifyPresent(present.get());
            present->DxgkPresentHistoryTokenData = 0;
            present->DwmNotified = true;

            mLastPresentByWindow[hwnd] = present;

            mPresentByDxgkPresentHistoryTokenData.erase(flipIter);
        }
        break;
    }
    case Microsoft_Windows_Dwm_Core::SCHEDULE_SURFACEUPDATE_Info::Id:
    {
        EventDataDesc desc[] = {
            { L"luidSurface" },
            { L"PresentCount" },
            { L"bindId" },
        };
        mMetadata.GetEventData(pEventRecord, desc, _countof(desc));
        auto luidSurface  = desc[0].GetData<uint64_t>();
        auto PresentCount = desc[1].GetData<uint64_t>();
        auto bindId       = desc[2].GetData<uint64_t>();

        Win32KPresentHistoryToken key(luidSurface, PresentCount, bindId);
        auto eventIter = mPresentByWin32KPresentHistoryToken.find(key);
        if (eventIter != mPresentByWin32KPresentHistoryToken.end() && eventIter->second->SeenInFrameEvent) {
            TRACK_PRESENT_PATH(eventIter->second);
            DebugModifyPresent(eventIter->second.get());
            eventIter->second->DwmNotified = true;
            mPresentsWaitingForDWM.emplace_back(eventIter->second);
            eventIter->second->PresentInDwmWaitingStruct = true;
        }
        break;
    }
    default:
        assert(!mFilteredEvents || // Assert that filtering is working if expected
               hdr.ProviderId == Microsoft_Windows_Dwm_Core::Win7::GUID);
        break;
    }
}

// Remove the present from all temporary tracking structures.
void PMTraceConsumer::RemovePresentFromTemporaryTrackingCollections(std::shared_ptr<PresentEvent> p)
{
    // mAllPresents
    if (p->mAllPresentsTrackingIndex != UINT32_MAX) {
        mAllPresents[p->mAllPresentsTrackingIndex] = nullptr;
        p->mAllPresentsTrackingIndex = UINT32_MAX;
    }

    // mPresentByThreadId
    //
    // If the present was batched, it will by referenced in mPresentByThreadId
    // by both ThreadId and DriverBatchThreadId.
    //
    // We don't reset neither ThreadId nor DriverBatchThreadId as both are
    // useful outside of tracking.
    auto threadEventIter = mPresentByThreadId.find(p->ThreadId);
    if (threadEventIter != mPresentByThreadId.end() && threadEventIter->second == p) {
        mPresentByThreadId.erase(threadEventIter);
    }
    if (p->DriverBatchThreadId != 0) {
        threadEventIter = mPresentByThreadId.find(p->DriverBatchThreadId);
        if (threadEventIter != mPresentByThreadId.end() && threadEventIter->second == p) {
            mPresentByThreadId.erase(threadEventIter);
        }
    }

    // mOrderedPresentsByProcessId
    mOrderedPresentsByProcessId[p->ProcessId].erase(p->QpcTime);

    // mPresentBySubmitSequence
    if (p->QueueSubmitSequence != 0) {
        auto eventIter = mPresentBySubmitSequence.find(p->QueueSubmitSequence);
        if (eventIter != mPresentBySubmitSequence.end() && (eventIter->second == p)) {
            mPresentBySubmitSequence.erase(eventIter);
        }
        p->QueueSubmitSequence = 0;
    }

    // mPresentByWin32KPresentHistoryToken
    if (p->CompositionSurfaceLuid != 0) {
        Win32KPresentHistoryToken key(
            p->CompositionSurfaceLuid,
            p->Win32KPresentCount,
            p->Win32KBindId
        );

        auto eventIter = mPresentByWin32KPresentHistoryToken.find(key);
        if (eventIter != mPresentByWin32KPresentHistoryToken.end() && (eventIter->second == p)) {
            mPresentByWin32KPresentHistoryToken.erase(eventIter);
        }

        p->CompositionSurfaceLuid = 0;
        p->Win32KPresentCount = 0;
        p->Win32KBindId = 0;
    }

    // mPresentByDxgkPresentHistoryToken
    if (p->DxgkPresentHistoryToken != 0) {
        auto eventIter = mPresentByDxgkPresentHistoryToken.find(p->DxgkPresentHistoryToken);
        if (eventIter != mPresentByDxgkPresentHistoryToken.end() && eventIter->second == p) {
            mPresentByDxgkPresentHistoryToken.erase(eventIter);
        }
        p->DxgkPresentHistoryToken = 0;
    }

    // mPresentByDxgkPresentHistoryTokenData
    if (p->DxgkPresentHistoryTokenData != 0) {
        auto eventIter = mPresentByDxgkPresentHistoryTokenData.find(p->DxgkPresentHistoryTokenData);
        if (eventIter != mPresentByDxgkPresentHistoryTokenData.end() && eventIter->second == p) {
            mPresentByDxgkPresentHistoryTokenData.erase(eventIter);
        }
        p->DxgkPresentHistoryTokenData = 0;
    }

    // mPresentByDxgkContext
    if (p->DxgkContext != 0) {
        auto eventIter = mPresentByDxgkContext.find(p->DxgkContext);
        if (eventIter != mPresentByDxgkContext.end() && eventIter->second == p) {
            mPresentByDxgkContext.erase(eventIter);
        }
        p->DxgkContext = 0;
    }

    // mLastPresentByWindow
    if (p->Hwnd != 0) {
        auto eventIter = mLastPresentByWindow.find(p->Hwnd);
        if (eventIter != mLastPresentByWindow.end() && eventIter->second == p) {
            mLastPresentByWindow.erase(eventIter);
        }
        p->Hwnd = 0;
    }

    // mPresentsWaitingForDWM
    if (p->PresentInDwmWaitingStruct) {
        for (auto presentIter = mPresentsWaitingForDWM.begin(); presentIter != mPresentsWaitingForDWM.end(); presentIter++) {
            if (p == *presentIter) {
                mPresentsWaitingForDWM.erase(presentIter);
                break;
            }
        }
        p->PresentInDwmWaitingStruct = false;
    }
}

void PMTraceConsumer::RemoveLostPresent(std::shared_ptr<PresentEvent> p)
{
    DebugModifyPresent(p.get());
    p->IsLost = true;
    CompletePresent(p);
}

void PMTraceConsumer::CompletePresentHelper(std::shared_ptr<PresentEvent> const& p)
{
    // First, protect against double-completion.  Double-completion is not
    // intended, but there have been cases observed where it happens (in some
    // cases leading to infinite recursion with a DWM present and a dependent
    // present completing eachother).
    //
    // The exact pattern causing this has not yet been identified, so is the
    // best fix for now.
    DebugAssert(p->IsCompleted == false);
    if (p->IsCompleted) return;

    // Complete the present.
    DebugModifyPresent(p.get());
    p->IsCompleted = true;
    mDeferredCompletions[p->ProcessId][p->SwapChainAddress].mOrderedPresents.emplace(p->QpcTime, p);

    // If the present is still missing some expected events, defer it's
    // enqueuing for some number of presents for cases where the event may
    // arrive after present completion.
    //
    // These cases must have special code to patch complete-but-deferred
    // presents, which are no longer in the standard tracking structures.
    if (!p->IsLost) {
        p->DeferredCompletionWaitCount = GetDeferredCompletionWaitCount(*p);
    }

    // Remove the present from any tracking structures.
    DebugModifyPresent(nullptr); // Prevent debug reporting of tracking removal
    RemovePresentFromTemporaryTrackingCollections(p);

    // If this is a DWM present, complete any other present that contributed to
    // it.  A DWM present only completes each HWND's most-recent Composed_Flip
    // PresentEvent, so we mark any others as discarded.
    //
    // PresentEvents that become lost are not removed from DependentPresents
    // tracking, so we need to protect against lost events (but they have
    // already been added to mLostPresentEvents etc.).
    if (!p->DependentPresents.empty()) {
        std::unordered_set<uint64_t> completedComposedFlipHwnds;
        for (auto ii = p->DependentPresents.rbegin(), ie = p->DependentPresents.rend(); ii != ie; ++ii) {
            auto p2 = *ii;
            if (!p2->IsCompleted) {
                if (p2->PresentMode == PresentMode::Composed_Flip && !completedComposedFlipHwnds.emplace(p2->Hwnd).second) {
                    DebugModifyPresent(p2.get());
                    p2->FinalState = PresentResult::Discarded;
                } else if (p2->FinalState != PresentResult::Discarded) {
                    DebugModifyPresent(p2.get());
                    p2->FinalState = p->FinalState;
                    p2->ScreenTime = p->ScreenTime;
                }
            }
        }
        for (auto p2 : p->DependentPresents) {
            if (!p2->IsCompleted) {
                CompletePresentHelper(p2);
            }
        }
        p->DependentPresents.clear();
    }

    // If presented, remove any earlier presents made on the same swap chain.
    if (p->FinalState == PresentResult::Presented) {
        auto presentsByThisProcess = &mOrderedPresentsByProcessId[p->ProcessId];
        for (auto ii = presentsByThisProcess->begin(), ie = presentsByThisProcess->end(); ii != ie; ) {
            auto p2 = ii->second;
            ++ii; // increment iterator first as CompletePresentHelper() will remove it
            if (p2->SwapChainAddress == p->SwapChainAddress) {
                if (p2->QpcTime >= p->QpcTime) break;
                CompletePresentHelper(p2);
            }
        }
    }
}

void PMTraceConsumer::CompletePresent(std::shared_ptr<PresentEvent> const& p)
{
    // We use the first completed present to indicate that all necessary
    // providers are running and able to successfully track/complete presents.
    //
    // At the first completion, there may be numerous presents that have been
    // created but not properly tracked due to missed events.  This is
    // especially prevalent in ETLs that start runtime providers before backend
    // providers and/or start capturing while an intensive graphics application
    // is already running.  When that happens, QpcTime/TimeTaken and
    // ReadyTime/ScreenTime times can become mis-matched, and that offset can
    // persist for the full capture.
    //
    // We handle this by throwing away all queued presents up to this point.
    if (!mHasCompletedAPresent && !p->IsLost) {
        mHasCompletedAPresent = true;

        for (auto const& pr : mOrderedPresentsByProcessId) {
            auto processPresents = &pr.second;
            for (auto ii = processPresents->begin(), ie = processPresents->end(); ii != ie; ) {
                auto p2 = ii->second;
                ++ii; // Increment before calling CompletePresentHelper(), which removes from processPresents

                // Clear DependentPresents as an optimization to avoid the extra
                // recursion in CompletePresentHelper(), since we know that we're
                // completing all the presents anyway.
                p2->DependentPresents.clear();

                DebugModifyPresent(p2.get());
                p2->IsLost = true;
                CompletePresentHelper(p2);
            }
        }
    }

    // Complete the present and any of its dependencies.
    else {
        CompletePresentHelper(p);
    }

    // In order, move any completed presents into the consumer thread queue.
    for (auto& pr1 : mDeferredCompletions) {
        for (auto& pr2 : pr1.second) {
            EnqueueDeferredCompletions(&pr2.second);
        }
    }
}

void PMTraceConsumer::EnqueueDeferredPresent(std::shared_ptr<PresentEvent> const& p)
{
    auto i1 = mDeferredCompletions.find(p->ProcessId);
    if (i1 != mDeferredCompletions.end()) {
        auto i2 = i1->second.find(p->SwapChainAddress);
        if (i2 != i1->second.end()) {
            EnqueueDeferredCompletions(&i2->second);
        }
    }
}

void PMTraceConsumer::EnqueueDeferredCompletions(DeferredCompletions* deferredCompletions)
{
    size_t completedCount = 0;
    size_t lostCount = 0;

    auto iterBegin = deferredCompletions->mOrderedPresents.begin();
    auto iterEnqueueEnd = iterBegin;
    for (auto iterEnd = deferredCompletions->mOrderedPresents.end(); iterEnqueueEnd != iterEnd; ++iterEnqueueEnd) {
        auto present = iterEnqueueEnd->second;
        if (present->DeferredCompletionWaitCount > 0) {
            break;
        }

        // Assert the present is complete and has been removed from all
        // internal tracking structures.
        DebugAssert(present->IsCompleted);
        DebugAssert(present->CompositionSurfaceLuid == 0);
        DebugAssert(present->Win32KPresentCount == 0);
        DebugAssert(present->Win32KBindId == 0);
        DebugAssert(present->DxgkPresentHistoryToken == 0);
        DebugAssert(present->DxgkPresentHistoryTokenData == 0);
        DebugAssert(present->DxgkContext == 0);
        DebugAssert(present->Hwnd == 0);
        DebugAssert(present->mAllPresentsTrackingIndex == UINT32_MAX);
        DebugAssert(present->QueueSubmitSequence == 0);
        DebugAssert(present->PresentInDwmWaitingStruct == false);

        // If later presents have already be enqueued for the user, mark this
        // present as lost.
        if (deferredCompletions->mLastEnqueuedQpcTime > present->QpcTime) {
            present->IsLost = true;
        }

        if (present->IsLost) {
            lostCount += 1;
        } else {
            deferredCompletions->mLastEnqueuedQpcTime = present->QpcTime;
            completedCount += 1;
        }
    }

    if (lostCount + completedCount > 0) {
        if (completedCount > 0) {
            std::lock_guard<std::mutex> lock(mPresentEventMutex);
            mCompletePresentEvents.reserve(mCompletePresentEvents.size() + completedCount);
            for (auto iter = iterBegin; iter != iterEnqueueEnd; ++iter) {
                if (!iter->second->IsLost) {
                    mCompletePresentEvents.emplace_back(iter->second);
                }
            }
        }

        if (lostCount > 0) {
            std::lock_guard<std::mutex> lock(mLostPresentEventMutex);
            mLostPresentEvents.reserve(mLostPresentEvents.size() + lostCount);
            for (auto iter = iterBegin; iter != iterEnqueueEnd; ++iter) {
                if (iter->second->IsLost) {
                    mLostPresentEvents.emplace_back(iter->second);
                }
            }
        }

        deferredCompletions->mOrderedPresents.erase(iterBegin, iterEnqueueEnd);
    }
}

std::shared_ptr<PresentEvent> PMTraceConsumer::FindOrCreatePresent(EVENT_HEADER const& hdr)
{
    // Check if there is an in-progress present that this thread is already
    // working on and, if so, continue working on that.
    auto threadEventIter = mPresentByThreadId.find(hdr.ThreadId);
    if (threadEventIter != mPresentByThreadId.end()) {
        return threadEventIter->second;
    }

    // If not, check if this event is from a process that is filtered out and,
    // if so, ignore it.
    if (!IsProcessTrackedForFiltering(hdr.ProcessId)) {
        return nullptr;
    }

    // Search for an in-progress present created by this process that still
    // doesn't have a known PresentMode.  This can be the case for DXGI/D3D
    // presents created on a different thread, which are batched and then
    // handled later during a DXGK/Win32K event.  If found, we add it to
    // mPresentByThreadId to indicate what present this thread is working on.
    auto presentsByThisProcess = &mOrderedPresentsByProcessId[hdr.ProcessId];
    for (auto const& tuple : *presentsByThisProcess) {
        auto presentEvent = tuple.second;
        if (presentEvent->PresentMode == PresentMode::Unknown) {
            DebugAssert(presentEvent->DriverBatchThreadId == 0);
            DebugModifyPresent(presentEvent.get());
            presentEvent->DriverBatchThreadId = hdr.ThreadId;
            mPresentByThreadId.emplace(hdr.ThreadId, presentEvent);
            return presentEvent;
        }
    }

    // Because we couldn't find a present above, the calling event is for an
    // unknown, in-progress present.  This can happen if the present didn't
    // originate from a runtime whose events we're tracking (i.e., DXGI or
    // D3D9) in which case a DXGKRNL event will be the first present-related
    // event we ever see.  So, we create the PresentEvent and start tracking it
    // from here.
    auto presentEvent = std::make_shared<PresentEvent>(hdr, Runtime::Other);
    TrackPresent(presentEvent, presentsByThisProcess);
    return presentEvent;
}

void PMTraceConsumer::TrackPresent(
    std::shared_ptr<PresentEvent> present,
    OrderedPresents* presentsByThisProcess)
{
    // If there is an existing present that hasn't completed by the time the
    // circular buffer has come around, consider it lost.
    if (mAllPresents[mAllPresentsNextIndex] != nullptr) {
        RemoveLostPresent(mAllPresents[mAllPresentsNextIndex]);
    }

    DebugCreatePresent(*present);
    present->mAllPresentsTrackingIndex = mAllPresentsNextIndex;
    mAllPresents[mAllPresentsNextIndex] = present;
    mAllPresentsNextIndex = (mAllPresentsNextIndex + 1) % PRESENTEVENT_CIRCULAR_BUFFER_SIZE;

    presentsByThisProcess->emplace(present->QpcTime, present);
    mPresentByThreadId.emplace(present->ThreadId, present);

    // Assign any pending retrieved input to this frame
    if (mTrackInput) {
        auto ii = mRetrievedInput.find(present->ProcessId);
        if (ii != mRetrievedInput.end() && ii->second.first != 0) {
            DebugModifyPresent(present.get());
            present->InputTime = ii->second.first;
            present->InputType = ii->second.second;
            ii->second.first = 0;
            ii->second.second = InputDeviceType::Unknown;
        }
    }
}

void PMTraceConsumer::RuntimePresentStart(Runtime runtime, EVENT_HEADER const& hdr, uint64_t swapchainAddr,
                                          uint32_t dxgiPresentFlags, int32_t syncInterval)
{
    // If there is an in-flight present on this thread already, then something
    // has gone wrong with it's tracking so consider it lost.
    auto iter = mPresentByThreadId.find(hdr.ThreadId);
    if (iter != mPresentByThreadId.end()) {
        RemoveLostPresent(iter->second);
    }

    // Ignore PRESENT_TEST as it doesn't present, it's used to check if you're
    // fullscreen.
    if (dxgiPresentFlags & DXGI_PRESENT_TEST) {
        return;
    }

    auto present = std::make_shared<PresentEvent>(hdr, runtime);

    DebugModifyPresent(present.get());
    present->SwapChainAddress = swapchainAddr;
    present->PresentFlags     = dxgiPresentFlags;
    present->SyncInterval     = syncInterval;

    TRACK_PRESENT_PATH_SAVE_GENERATED_ID(present);

    TrackPresent(present, &mOrderedPresentsByProcessId[present->ProcessId]);
}

// No TRACK_PRESENT instrumentation here because each runtime Present::Start
// event is instrumented and we assume we'll see the corresponding Stop event
// for any completed present.
void PMTraceConsumer::RuntimePresentStop(Runtime runtime, EVENT_HEADER const& hdr, uint32_t result)
{
    // If the Present() call failed, lookup the PresentEvent as the one
    // most-recently operated on by the same thread.  If not found, ignore the
    // Present() stop event.  If found, we throw it away (i.e., it isn't
    // returned as either a completed nor lost present).
    if (FAILED(result)) {
        auto eventIter = mPresentByThreadId.find(hdr.ThreadId);
        if (eventIter != mPresentByThreadId.end()) {
            auto present = eventIter->second;

            // Check expected state (a new Present() that has only been started).
            DebugAssert(present->TimeTaken                   == 0);
            DebugAssert(present->IsCompleted                 == false);
            DebugAssert(present->IsLost                      == false);
            DebugAssert(present->DeferredCompletionWaitCount == 0);
            DebugAssert(present->DependentPresents.empty());

            // Remove the present from any tracking structures.
            DebugModifyPresent(nullptr); // Prevent debug reporting of tracking removal
            RemovePresentFromTemporaryTrackingCollections(present);
        }
        return;
    }

    // If there are any deferred presents for this process, decrement their
    // DeferredCompletionWaitCount and enqueue any that reach
    // DeferredCompletionWaitCount==0.
    //
    // One of the deferred cases is when a present is displayed/dropped before
    // Present() returns, so if any deferred presents have a missing
    // Present_Stop we use this Present_Stop for the oldest one.
    {
        bool presentStopUsed = false;

        auto iter = mDeferredCompletions.find(hdr.ProcessId);
        if (iter != mDeferredCompletions.end()) {
            for (auto& pr1 : iter->second) {
                auto deferredCompletions = &pr1.second;

                bool enqueuePresents = false;
                bool isOldest = true;
                for (auto const& pr2 : deferredCompletions->mOrderedPresents) {
                    auto present = pr2.second;
                    if (present->DeferredCompletionWaitCount > 0 && present->ThreadId == hdr.ThreadId && present->Runtime == runtime) {

                        DebugModifyPresent(present.get());
                        present->DeferredCompletionWaitCount -= 1;

                        if (!presentStopUsed && present->TimeTaken == 0) {
                            present->TimeTaken = *(uint64_t*) &hdr.TimeStamp - present->QpcTime;
                            if (GetDeferredCompletionWaitCount(*present) == 0) {
                                present->DeferredCompletionWaitCount = 0;
                            }
                            presentStopUsed = true;
                        }

                        if (present->DeferredCompletionWaitCount == 0 && isOldest) {
                            enqueuePresents = true;
                        }
                    }
                    isOldest = false;
                }

                if (enqueuePresents) {
                    EnqueueDeferredCompletions(deferredCompletions); 
                }
            }
        }
        if (presentStopUsed) {
            return;
        }
    }

    // Next, lookup the PresentEvent most-recently operated on by the same
    // thread.  If there isn't one, ignore this event.
    auto eventIter = mPresentByThreadId.find(hdr.ThreadId);
    if (eventIter != mPresentByThreadId.end()) {
        auto present = eventIter->second;

        DebugModifyPresent(present.get());
        present->Runtime   = runtime;
        present->TimeTaken = *(uint64_t*) &hdr.TimeStamp - present->QpcTime;

        bool allowBatching = false;
        switch (runtime) {
        case Runtime::DXGI:
            if (result != DXGI_STATUS_OCCLUDED &&
                result != DXGI_STATUS_MODE_CHANGE_IN_PROGRESS &&
                result != DXGI_STATUS_NO_DESKTOP_ACCESS) {
                allowBatching = true;
            }
            break;
        case Runtime::D3D9:
            if (result != S_PRESENT_OCCLUDED) {
                allowBatching = true;
            }
            break;
        }

        if (allowBatching && mTrackDisplay) {
            // We now remove this present from mPresentByThreadId because any future
            // event related to it (e.g., from DXGK/Win32K/etc.) is not expected to
            // come from this thread.
            mPresentByThreadId.erase(eventIter);
        } else {
            present->FinalState = allowBatching ? PresentResult::Presented : PresentResult::Discarded;
            CompletePresent(present);
        }
    }
}

void PMTraceConsumer::HandleNTProcessEvent(EVENT_RECORD* pEventRecord)
{
    if (pEventRecord->EventHeader.EventDescriptor.Opcode == EVENT_TRACE_TYPE_START ||
        pEventRecord->EventHeader.EventDescriptor.Opcode == EVENT_TRACE_TYPE_DC_START ||
        pEventRecord->EventHeader.EventDescriptor.Opcode == EVENT_TRACE_TYPE_END||
        pEventRecord->EventHeader.EventDescriptor.Opcode == EVENT_TRACE_TYPE_DC_END) {
        EventDataDesc desc[] = {
            { L"ProcessId" },
            { L"ImageFileName" },
        };
        mMetadata.GetEventData(pEventRecord, desc, _countof(desc));

        ProcessEvent event;
        event.QpcTime       = pEventRecord->EventHeader.TimeStamp.QuadPart;
        event.ProcessId     = desc[0].GetData<uint32_t>();
        event.ImageFileName = desc[1].GetData<std::string>();
        event.IsStartEvent  = pEventRecord->EventHeader.EventDescriptor.Opcode == EVENT_TRACE_TYPE_START ||
                              pEventRecord->EventHeader.EventDescriptor.Opcode == EVENT_TRACE_TYPE_DC_START;

        if (event.IsStartEvent) {
            mNTProcessNames[event.ProcessId] = event.ImageFileName;
        } else {
            mNTProcessNames.erase(event.ProcessId);
        }

        std::lock_guard<std::mutex> lock(mProcessEventMutex);
        mProcessEvents.emplace_back(event);
        return;
    }
}

void PMTraceConsumer::HandleMetadataEvent(EVENT_RECORD* pEventRecord)
{
    mMetadata.AddMetadata(pEventRecord);
}

void PMTraceConsumer::AddTrackedProcessForFiltering(uint32_t processID)
{
    std::unique_lock<std::shared_mutex> lock(mTrackedProcessFilterMutex);
    mTrackedProcessFilter.insert(processID);
}

void PMTraceConsumer::RemoveTrackedProcessForFiltering(uint32_t processID)
{
    std::unique_lock<std::shared_mutex> lock(mTrackedProcessFilterMutex);
    auto iterator = mTrackedProcessFilter.find(processID);
    if (iterator != mTrackedProcessFilter.end()) {
        mTrackedProcessFilter.erase(iterator);
    }

    // Completion events will remove any currently tracked events for this process
    // from data structures, so we don't need to proactively remove them now.
}

bool PMTraceConsumer::IsProcessTrackedForFiltering(uint32_t processID)
{
    if (!mFilteredProcessIds || processID == DwmProcessId) {
        return true;
    }

    std::shared_lock<std::shared_mutex> lock(mTrackedProcessFilterMutex);
    auto iterator = mTrackedProcessFilter.find(processID);
    return (iterator != mTrackedProcessFilter.end());
}

#ifdef TRACK_PRESENT_PATHS
static_assert(__COUNTER__ <= 64, "Too many TRACK_PRESENT ids to store in PresentEvent::AnalysisPath");
#endif
