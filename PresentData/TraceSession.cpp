// Copyright (C) 2020-2021 Intel Corporation
// SPDX-License-Identifier: MIT

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <assert.h>
#include <stddef.h>
#include <windows.h>
#include <evntcons.h> // must include after windows.h

#include "TraceSession.hpp"

#include "Debug.hpp"
#include "PresentMonTraceConsumer.hpp"
#include "MixedRealityTraceConsumer.hpp"

#include "ETW/Microsoft_Windows_D3D9.h"
#include "ETW/Microsoft_Windows_Dwm_Core.h"
#include "ETW/Microsoft_Windows_DXGI.h"
#include "ETW/Microsoft_Windows_DxgKrnl.h"
#include "ETW/Microsoft_Windows_EventMetadata.h"
#include "ETW/Microsoft_Windows_Win32k.h"
#include "ETW/NT_Process.h"

namespace {

struct TraceProperties : public EVENT_TRACE_PROPERTIES {
    wchar_t mSessionName[MAX_PATH];
};

ULONG EnableFilteredProvider(
    TRACEHANDLE sessionHandle,
    GUID const& sessionGuid, GUID const& providerGuid, UCHAR level,
    ULONGLONG anyKeywordMask, ULONGLONG allKeywordMask,
    std::vector<USHORT> const& eventIds)
{
    assert(eventIds.size() >= ANYSIZE_ARRAY);
    assert(eventIds.size() <= MAX_EVENT_FILTER_EVENT_ID_COUNT);
    auto memorySize = sizeof(EVENT_FILTER_EVENT_ID) + sizeof(USHORT) * (eventIds.size() - ANYSIZE_ARRAY);
    auto memory = _aligned_malloc(memorySize, alignof(USHORT));
    if (memory == nullptr) {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    auto filterEventIds = (EVENT_FILTER_EVENT_ID*) memory;
    filterEventIds->FilterIn = TRUE;
    filterEventIds->Reserved = 0;
    filterEventIds->Count = 0;
    for (auto id : eventIds) {
        filterEventIds->Events[filterEventIds->Count++] = id;
    }

    EVENT_FILTER_DESCRIPTOR filterDesc = {};
    filterDesc.Ptr = (ULONGLONG) filterEventIds;
    filterDesc.Size = (ULONG) memorySize;
    filterDesc.Type = EVENT_FILTER_TYPE_EVENT_ID;

    ENABLE_TRACE_PARAMETERS params = {};
    params.Version = ENABLE_TRACE_PARAMETERS_VERSION_2;
    params.EnableProperty = EVENT_ENABLE_PROPERTY_IGNORE_KEYWORD_0;
    params.SourceId = sessionGuid;
    params.EnableFilterDesc = &filterDesc;
    params.FilterDescCount = 1;

    ULONG timeout = 0;
    auto status = EnableTraceEx2(sessionHandle, &providerGuid, EVENT_CONTROL_CODE_ENABLE_PROVIDER, level, anyKeywordMask, allKeywordMask, timeout, &params);
    _aligned_free(memory);
    return status;
}

ULONG EnableProviders(
    TRACEHANDLE sessionHandle,
    GUID const& sessionGuid,
    PMTraceConsumer* pmConsumer,
    MRTraceConsumer* mrConsumer)
{
    uint64_t anyKeywordMask = 0;
    uint64_t allKeywordMask = 0;
    std::vector<USHORT> eventIds;
    ULONG status = 0;

    // Start backend providers first to reduce Presents being queued up before
    // we can track them.

    // Microsoft_Windows_DxgKrnl
    anyKeywordMask =
        (uint64_t) Microsoft_Windows_DxgKrnl::Keyword::Microsoft_Windows_DxgKrnl_Performance |
        (uint64_t) Microsoft_Windows_DxgKrnl::Keyword::Base;
    allKeywordMask = anyKeywordMask;
    eventIds = {
        Microsoft_Windows_DxgKrnl::PresentHistory_Start::Id,
    };
    if (pmConsumer->mTrackDisplay) {
        eventIds.push_back(Microsoft_Windows_DxgKrnl::Blit_Info::Id);
        eventIds.push_back(Microsoft_Windows_DxgKrnl::Flip_Info::Id);
        eventIds.push_back(Microsoft_Windows_DxgKrnl::FlipMultiPlaneOverlay_Info::Id);
        eventIds.push_back(Microsoft_Windows_DxgKrnl::HSyncDPCMultiPlane_Info::Id);
        eventIds.push_back(Microsoft_Windows_DxgKrnl::VSyncDPCMultiPlane_Info::Id);
        eventIds.push_back(Microsoft_Windows_DxgKrnl::MMIOFlip_Info::Id);
        eventIds.push_back(Microsoft_Windows_DxgKrnl::MMIOFlipMultiPlaneOverlay_Info::Id);
        eventIds.push_back(Microsoft_Windows_DxgKrnl::Present_Info::Id);
        eventIds.push_back(Microsoft_Windows_DxgKrnl::PresentHistory_Info::Id);
        eventIds.push_back(Microsoft_Windows_DxgKrnl::PresentHistoryDetailed_Start::Id);
        eventIds.push_back(Microsoft_Windows_DxgKrnl::QueuePacket_Start::Id);
        eventIds.push_back(Microsoft_Windows_DxgKrnl::QueuePacket_Stop::Id);
        eventIds.push_back(Microsoft_Windows_DxgKrnl::VSyncDPC_Info::Id);
    }
    if (pmConsumer->mTrackGPU) {
        anyKeywordMask |=
            (uint64_t) Microsoft_Windows_DxgKrnl::Keyword::LongHaul |
            (uint64_t) Microsoft_Windows_DxgKrnl::Keyword::Resource;
        allKeywordMask =
            (uint64_t) Microsoft_Windows_DxgKrnl::Keyword::Microsoft_Windows_DxgKrnl_Performance;

        eventIds.push_back(Microsoft_Windows_DxgKrnl::Context_DCStart::Id);
        eventIds.push_back(Microsoft_Windows_DxgKrnl::Context_Start::Id);
        eventIds.push_back(Microsoft_Windows_DxgKrnl::Context_Stop::Id);
        eventIds.push_back(Microsoft_Windows_DxgKrnl::Device_DCStart::Id);
        eventIds.push_back(Microsoft_Windows_DxgKrnl::Device_Start::Id);
        eventIds.push_back(Microsoft_Windows_DxgKrnl::Device_Stop::Id);
        eventIds.push_back(Microsoft_Windows_DxgKrnl::DmaPacket_Info::Id);
        eventIds.push_back(Microsoft_Windows_DxgKrnl::DmaPacket_Start::Id);
    }
    status = EnableFilteredProvider(sessionHandle, sessionGuid, Microsoft_Windows_DxgKrnl::GUID, TRACE_LEVEL_INFORMATION, anyKeywordMask, allKeywordMask, eventIds);
    if (status != ERROR_SUCCESS) return status;

    if (pmConsumer->mTrackGPU) {
        // Request state capture on DxgKrnl so we see Context_DCStart/Device_DCStart
        // events for existing contexts/devices.
        status = EnableTraceEx2(sessionHandle, &Microsoft_Windows_DxgKrnl::GUID, EVENT_CONTROL_CODE_CAPTURE_STATE,
                                TRACE_LEVEL_INFORMATION, anyKeywordMask, allKeywordMask, 0, nullptr);
        if (status != ERROR_SUCCESS) return status;
    }

    status = EnableTraceEx2(sessionHandle, &Microsoft_Windows_DxgKrnl::Win7::GUID, EVENT_CONTROL_CODE_ENABLE_PROVIDER,
                            TRACE_LEVEL_INFORMATION, anyKeywordMask, allKeywordMask, 0, nullptr);
    if (status != ERROR_SUCCESS) return status;

    if (pmConsumer->mTrackDisplay) {
        // Microsoft_Windows_Win32k
        anyKeywordMask =
            (uint64_t) Microsoft_Windows_Win32k::Keyword::Updates |
            (uint64_t) Microsoft_Windows_Win32k::Keyword::Visualization |
            (uint64_t) Microsoft_Windows_Win32k::Keyword::Microsoft_Windows_Win32k_Tracing;
        allKeywordMask =
            (uint64_t) Microsoft_Windows_Win32k::Keyword::Updates |
            (uint64_t) Microsoft_Windows_Win32k::Keyword::Microsoft_Windows_Win32k_Tracing;
        eventIds = {
            Microsoft_Windows_Win32k::TokenCompositionSurfaceObject_Info::Id,
            Microsoft_Windows_Win32k::TokenStateChanged_Info::Id,
        };

        if (pmConsumer->mTrackInputs) {
            anyKeywordMask = anyKeywordMask | (uint64_t) Microsoft_Windows_Win32k::Keyword::Microsoft_Windows_Win32k_Messages
                                            | (uint64_t) Microsoft_Windows_Win32k::Keyword::MessagePump
                                            | (uint64_t) Microsoft_Windows_Win32k::Keyword::MessagePumpInternalAndInput
                                            | (uint64_t) Microsoft_Windows_Win32k::Keyword::ComponentHosting;
            allKeywordMask = 0;

            eventIds.push_back(Microsoft_Windows_Win32k::InputDeviceRead_Stop::Id);
            eventIds.push_back(Microsoft_Windows_Win32k::RetrieveInputMessage_Info::Id);
        }

        status = EnableFilteredProvider(sessionHandle, sessionGuid, Microsoft_Windows_Win32k::GUID, TRACE_LEVEL_INFORMATION, anyKeywordMask, allKeywordMask, eventIds);
        if (status != ERROR_SUCCESS) return status;

        // Microsoft_Windows_Dwm_Core
        anyKeywordMask = 0;
        allKeywordMask = anyKeywordMask;
        eventIds = {
            Microsoft_Windows_Dwm_Core::MILEVENT_MEDIA_UCE_PROCESSPRESENTHISTORY_GetPresentHistory_Info::Id,
            Microsoft_Windows_Dwm_Core::SCHEDULE_PRESENT_Start::Id,
            Microsoft_Windows_Dwm_Core::SCHEDULE_SURFACEUPDATE_Info::Id,
            Microsoft_Windows_Dwm_Core::FlipChain_Pending::Id,
            Microsoft_Windows_Dwm_Core::FlipChain_Complete::Id,
            Microsoft_Windows_Dwm_Core::FlipChain_Dirty::Id,
        };
        status = EnableFilteredProvider(sessionHandle, sessionGuid, Microsoft_Windows_Dwm_Core::GUID, TRACE_LEVEL_VERBOSE, anyKeywordMask, allKeywordMask, eventIds);
        if (status != ERROR_SUCCESS) return status;

        status = EnableTraceEx2(sessionHandle, &Microsoft_Windows_Dwm_Core::Win7::GUID, EVENT_CONTROL_CODE_ENABLE_PROVIDER,
                                TRACE_LEVEL_VERBOSE, 0, 0, 0, nullptr);
        if (status != ERROR_SUCCESS) return status;
    }

    // Microsoft_Windows_DXGI
    anyKeywordMask =
        (uint64_t) Microsoft_Windows_DXGI::Keyword::Microsoft_Windows_DXGI_Analytic |
        (uint64_t) Microsoft_Windows_DXGI::Keyword::Events;
    allKeywordMask = anyKeywordMask;
    eventIds = {
        Microsoft_Windows_DXGI::Present_Start::Id,
        Microsoft_Windows_DXGI::Present_Stop::Id,
        Microsoft_Windows_DXGI::PresentMultiplaneOverlay_Start::Id,
        Microsoft_Windows_DXGI::PresentMultiplaneOverlay_Stop::Id,
    };
    status = EnableFilteredProvider(sessionHandle, sessionGuid, Microsoft_Windows_DXGI::GUID, TRACE_LEVEL_INFORMATION, anyKeywordMask, allKeywordMask, eventIds);
    if (status != ERROR_SUCCESS) return status;

    // Microsoft_Windows_D3D9
    anyKeywordMask =
        (uint64_t) Microsoft_Windows_D3D9::Keyword::Microsoft_Windows_Direct3D9_Analytic |
        (uint64_t) Microsoft_Windows_D3D9::Keyword::Events;
    allKeywordMask = anyKeywordMask;
    eventIds = {
        Microsoft_Windows_D3D9::Present_Start::Id,
        Microsoft_Windows_D3D9::Present_Stop::Id,
    };
    status = EnableFilteredProvider(sessionHandle, sessionGuid, Microsoft_Windows_D3D9::GUID, TRACE_LEVEL_INFORMATION, anyKeywordMask, allKeywordMask, eventIds);
    if (status != ERROR_SUCCESS) return status;

    if (mrConsumer != nullptr) {
        // DHD
        status = EnableTraceEx2(sessionHandle, &DHD_PROVIDER_GUID, EVENT_CONTROL_CODE_ENABLE_PROVIDER,
                                TRACE_LEVEL_VERBOSE, 0x1C00000, 0, 0, nullptr);
        if (status != ERROR_SUCCESS) return status;

        if (!mrConsumer->mSimpleMode) {
            // SPECTRUMCONTINUOUS
            status = EnableTraceEx2(sessionHandle, &SPECTRUMCONTINUOUS_PROVIDER_GUID, EVENT_CONTROL_CODE_ENABLE_PROVIDER,
                                    TRACE_LEVEL_VERBOSE, 0x800000, 0, 0, nullptr);
            if (status != ERROR_SUCCESS) return status;
        }
    }

    return ERROR_SUCCESS;
}

void DisableProviders(TRACEHANDLE sessionHandle)
{
    ULONG status = 0;
    status = EnableTraceEx2(sessionHandle, &Microsoft_Windows_DXGI::GUID,           EVENT_CONTROL_CODE_DISABLE_PROVIDER, 0, 0, 0, 0, nullptr);
    status = EnableTraceEx2(sessionHandle, &Microsoft_Windows_D3D9::GUID,           EVENT_CONTROL_CODE_DISABLE_PROVIDER, 0, 0, 0, 0, nullptr);
    status = EnableTraceEx2(sessionHandle, &Microsoft_Windows_DxgKrnl::GUID,        EVENT_CONTROL_CODE_DISABLE_PROVIDER, 0, 0, 0, 0, nullptr);
    status = EnableTraceEx2(sessionHandle, &Microsoft_Windows_Win32k::GUID,         EVENT_CONTROL_CODE_DISABLE_PROVIDER, 0, 0, 0, 0, nullptr);
    status = EnableTraceEx2(sessionHandle, &Microsoft_Windows_Dwm_Core::GUID,       EVENT_CONTROL_CODE_DISABLE_PROVIDER, 0, 0, 0, 0, nullptr);
    status = EnableTraceEx2(sessionHandle, &Microsoft_Windows_Dwm_Core::Win7::GUID, EVENT_CONTROL_CODE_DISABLE_PROVIDER, 0, 0, 0, 0, nullptr);
    status = EnableTraceEx2(sessionHandle, &Microsoft_Windows_DxgKrnl::Win7::GUID,  EVENT_CONTROL_CODE_DISABLE_PROVIDER, 0, 0, 0, 0, nullptr);
    status = EnableTraceEx2(sessionHandle, &DHD_PROVIDER_GUID,                      EVENT_CONTROL_CODE_DISABLE_PROVIDER, 0, 0, 0, 0, nullptr);
    status = EnableTraceEx2(sessionHandle, &SPECTRUMCONTINUOUS_PROVIDER_GUID,       EVENT_CONTROL_CODE_DISABLE_PROVIDER, 0, 0, 0, 0, nullptr);
}

template<
    bool SAVE_FIRST_TIMESTAMP,
    bool TRACK_DISPLAY,
    bool TRACK_INPUTS,
    bool WMR>
void CALLBACK EventRecordCallback(EVENT_RECORD* pEventRecord)
{
    auto session = (TraceSession*) pEventRecord->UserContext;
    auto const& hdr = pEventRecord->EventHeader;

    #pragma warning(push)
    #pragma warning(disable: 4984) // c++17 extension

    if constexpr (SAVE_FIRST_TIMESTAMP) {
        if (session->mStartQpc.QuadPart == 0) {
            session->mStartQpc = hdr.TimeStamp;
        }
    }

    if (hdr.ProviderId == Microsoft_Windows_DxgKrnl::GUID) {
        session->mPMConsumer->HandleDXGKEvent(pEventRecord);
        return;
    }
    if (hdr.ProviderId == Microsoft_Windows_DXGI::GUID) {
        session->mPMConsumer->HandleDXGIEvent(pEventRecord);
        return;
    }
    if (hdr.ProviderId == Microsoft_Windows_D3D9::GUID) {
        session->mPMConsumer->HandleD3D9Event(pEventRecord);
        return;
    }
    if (hdr.ProviderId == NT_Process::GUID) {
        session->mPMConsumer->HandleNTProcessEvent(pEventRecord);
        return;
    }
    if (hdr.ProviderId == Microsoft_Windows_DxgKrnl::Win7::PRESENTHISTORY_GUID) {
        session->mPMConsumer->HandleWin7DxgkPresentHistory(pEventRecord);
        return;
    }
    if (hdr.ProviderId == Microsoft_Windows_EventMetadata::GUID) {
        session->mPMConsumer->HandleMetadataEvent(pEventRecord);
        return;
    }

    if constexpr (TRACK_DISPLAY || TRACK_INPUTS) {
        if (hdr.ProviderId == Microsoft_Windows_Win32k::GUID) {
            session->mPMConsumer->HandleWin32kEvent(pEventRecord);
            return;
        }
    }

    if constexpr (TRACK_DISPLAY) {
        if (hdr.ProviderId == Microsoft_Windows_Dwm_Core::GUID) {
            session->mPMConsumer->HandleDWMEvent(pEventRecord);
            return;
        }
        if (hdr.ProviderId == Microsoft_Windows_Dwm_Core::Win7::GUID) {
            session->mPMConsumer->HandleDWMEvent(pEventRecord);
            return;
        }
        if (hdr.ProviderId == Microsoft_Windows_DxgKrnl::Win7::BLT_GUID) {
            session->mPMConsumer->HandleWin7DxgkBlt(pEventRecord);
            return;
        }
        if (hdr.ProviderId == Microsoft_Windows_DxgKrnl::Win7::FLIP_GUID) {
            session->mPMConsumer->HandleWin7DxgkFlip(pEventRecord);
            return;
        }
        if (hdr.ProviderId == Microsoft_Windows_DxgKrnl::Win7::QUEUEPACKET_GUID) {
            session->mPMConsumer->HandleWin7DxgkQueuePacket(pEventRecord);
            return;
        }
        if (hdr.ProviderId == Microsoft_Windows_DxgKrnl::Win7::VSYNCDPC_GUID) {
            session->mPMConsumer->HandleWin7DxgkVSyncDPC(pEventRecord);
            return;
        }
        if (hdr.ProviderId == Microsoft_Windows_DxgKrnl::Win7::MMIOFLIP_GUID) {
            session->mPMConsumer->HandleWin7DxgkMMIOFlip(pEventRecord);
            return;
        }

        if constexpr (WMR) {
            if (hdr.ProviderId == SPECTRUMCONTINUOUS_PROVIDER_GUID) {
                session->mMRConsumer->HandleSpectrumContinuousEvent(pEventRecord);
                return;
            }
        }
    }

    if constexpr (WMR) {
        if (hdr.ProviderId == DHD_PROVIDER_GUID) {
            session->mMRConsumer->HandleDHDEvent(pEventRecord);
            return;
        }
    }

    #pragma warning(pop)
}

ULONG CALLBACK BufferCallback(EVENT_TRACE_LOGFILEA* pLogFile)
{
    auto session = (TraceSession*) pLogFile->Context;
    return session->mContinueProcessingBuffers; // TRUE = continue processing events, FALSE = return out of ProcessTrace()
}

}

ULONG TraceSession::Start(
    PMTraceConsumer* pmConsumer,
    MRTraceConsumer* mrConsumer,
    char const* etlPath,
    char const* sessionName)
{
    assert(mSessionHandle == 0);
    assert(mTraceHandle == INVALID_PROCESSTRACE_HANDLE);
    mStartQpc.QuadPart = 0;
    mPMConsumer = pmConsumer;
    mMRConsumer = mrConsumer;
    mContinueProcessingBuffers = TRUE;

    // -------------------------------------------------------------------------
    // Configure trace properties
    EVENT_TRACE_LOGFILEA traceProps = {};
    traceProps.LogFileName = (char*) etlPath;
    traceProps.ProcessTraceMode = PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_RAW_TIMESTAMP;
    traceProps.Context = this;
    /* Output members (passed also to BufferCallback):
    traceProps.CurrentTime
    traceProps.BuffersRead
    traceProps.CurrentEvent
    traceProps.LogfileHeader
    traceProps.BufferSize
    traceProps.Filled
    traceProps.IsKernelTrace
    */

    // Redirect to a specialized event handler
    auto saveFirstTimestamp = etlPath != nullptr;
    auto trackDisplay       = pmConsumer->mTrackDisplay;
    auto trackInputs        = pmConsumer->mTrackInputs;
    auto trackWMR           = mrConsumer != nullptr;

    UINT callbackFlags =
        (saveFirstTimestamp ? 8 : 0) |
        (trackDisplay       ? 4 : 0) |
        (trackInputs        ? 2 : 0) |
        (trackWMR           ? 1 : 0);
    switch (callbackFlags) {
    case  0: traceProps.EventRecordCallback = &EventRecordCallback<false, false, false, false>; break;
    case  1: traceProps.EventRecordCallback = &EventRecordCallback<false, false, false,  true>; break;
    case  2: traceProps.EventRecordCallback = &EventRecordCallback<false, false,  true, false>; break;
    case  3: traceProps.EventRecordCallback = &EventRecordCallback<false, false,  true,  true>; break;
    case  4: traceProps.EventRecordCallback = &EventRecordCallback<false,  true, false, false>; break;
    case  5: traceProps.EventRecordCallback = &EventRecordCallback<false,  true, false,  true>; break;
    case  6: traceProps.EventRecordCallback = &EventRecordCallback<false,  true,  true, false>; break;
    case  7: traceProps.EventRecordCallback = &EventRecordCallback<false,  true,  true,  true>; break;
    case  8: traceProps.EventRecordCallback = &EventRecordCallback< true, false, false, false>; break;
    case  9: traceProps.EventRecordCallback = &EventRecordCallback< true, false, false,  true>; break;
    case 10: traceProps.EventRecordCallback = &EventRecordCallback< true, false,  true, false>; break;
    case 11: traceProps.EventRecordCallback = &EventRecordCallback< true, false,  true,  true>; break;
    case 12: traceProps.EventRecordCallback = &EventRecordCallback< true,  true, false, false>; break;
    case 13: traceProps.EventRecordCallback = &EventRecordCallback< true,  true, false,  true>; break;
    case 14: traceProps.EventRecordCallback = &EventRecordCallback< true,  true,  true, false>; break;
    case 15: traceProps.EventRecordCallback = &EventRecordCallback< true,  true,  true,  true>; break;
    }

    // When processing log files, we need to use the buffer callback in case
    // the user wants to stop processing before the entire log has been parsed.
    if (traceProps.LogFileName != nullptr) {
        traceProps.BufferCallback = &BufferCallback;
    }

    // -------------------------------------------------------------------------
    // For realtime collection, start the session with the required providers
    if (traceProps.LogFileName == nullptr) {
        traceProps.LoggerName = (char*) sessionName;
        traceProps.ProcessTraceMode |= PROCESS_TRACE_MODE_REAL_TIME;

        TraceProperties sessionProps = {};
        sessionProps.Wnode.BufferSize = (ULONG) sizeof(TraceProperties);
        sessionProps.Wnode.ClientContext = 1;                     // Clock resolution to use when logging the timestamp for each event; 1 == query performance counter
        sessionProps.LogFileMode = EVENT_TRACE_REAL_TIME_MODE;    // We have a realtime consumer, not writing to a log file
        sessionProps.LogFileNameOffset = 0;                       // 0 means no output log file
        sessionProps.LoggerNameOffset = offsetof(TraceProperties, mSessionName);  // Location of session name; will be written by StartTrace()
        /* Not used:
        sessionProps.Wnode.Guid               // Only needed for private or kernel sessions, otherwise it's an output
        sessionProps.FlushTimer               // How often in seconds buffers are flushed; 0=min (1 second)
        sessionProps.EnableFlags              // Which kernel providers to include in trace
        sessionProps.AgeLimit                 // n/a
        sessionProps.BufferSize = 0;          // Size of each tracing buffer in kB (max 1MB)
        sessionProps.MinimumBuffers = 200;    // Min tracing buffer pool size; must be at least 2 per processor
        sessionProps.MaximumBuffers = 0;      // Max tracing buffer pool size; min+20 by default
        sessionProps.MaximumFileSize = 0;     // Max file size in MB
        */
        /* The following members are output variables, set by StartTrace() and/or ControlTrace()
        sessionProps.Wnode.HistoricalContext  // handle to the event tracing session
        sessionProps.Wnode.TimeStamp          // time this structure was updated
        sessionProps.Wnode.Guid               // session Guid
        sessionProps.Wnode.Flags              // e.g., WNODE_FLAG_TRACED_GUID
        sessionProps.NumberOfBuffers          // trace buffer pool size
        sessionProps.FreeBuffers              // trace buffer pool free count
        sessionProps.EventsLost               // count of events not written
        sessionProps.BuffersWritten           // buffers written in total
        sessionProps.LogBuffersLost           // buffers that couldn't be written to the log
        sessionProps.RealTimeBuffersLost      // buffers that couldn't be delivered to the realtime consumer
        sessionProps.LoggerThreadId           // tracing session identifier
        */

        auto status = StartTraceA(&mSessionHandle, sessionName, &sessionProps);
        if (status != ERROR_SUCCESS) {
            mSessionHandle = 0;
            return status;
        }

        status = EnableProviders(mSessionHandle, sessionProps.Wnode.Guid, pmConsumer, mrConsumer);
        if (status != ERROR_SUCCESS) {
            Stop();
            return status;
        }
    }

    // -------------------------------------------------------------------------
    // Open the trace
    mTraceHandle = OpenTraceA(&traceProps);
    if (mTraceHandle == INVALID_PROCESSTRACE_HANDLE) {
        auto lastError = GetLastError();
        Stop();
        return lastError;
    }

    // -------------------------------------------------------------------------
    // Save the initial time to base capture off of.  ETL captures use the
    // time of the first event, which matches GPUVIEW usage, and realtime
    // captures are based off the timestamp here.

    switch (traceProps.LogfileHeader.ReservedFlags) {
    case 2: // System time
        mQpcFrequency.QuadPart = 10000000ull;
        break;
    case 3: // CPU cycle counter
        mQpcFrequency.QuadPart = 1000000ull * traceProps.LogfileHeader.CpuSpeedInMHz;
        break;
    default: // 1 == QPC
        mQpcFrequency = traceProps.LogfileHeader.PerfFreq;
        break;
    }

    if (saveFirstTimestamp) {
        SYSTEMTIME ust = {};
        SYSTEMTIME lst = {};
        FileTimeToSystemTime((FILETIME const*) &traceProps.LogfileHeader.StartTime, &ust);
        SystemTimeToTzSpecificLocalTime(&traceProps.LogfileHeader.TimeZone, &ust, &lst);
        SystemTimeToFileTime(&lst, &mStartTime);
    } else {
        LARGE_INTEGER qpc1 = {};
        LARGE_INTEGER qpc2 = {};
        FILETIME ft = {};
        QueryPerformanceCounter(&qpc1);
        GetSystemTimeAsFileTime(&ft);
        QueryPerformanceCounter(&qpc2);
        FileTimeToLocalFileTime(&ft, &mStartTime);
        mStartQpc.QuadPart = qpc1.QuadPart + (qpc2.QuadPart - qpc1.QuadPart) / 2;
    }

    DebugInitialize(&mStartQpc, mQpcFrequency);

    return ERROR_SUCCESS;
}

void TraceSession::Stop()
{
    ULONG status = 0;

    // If collecting realtime events, CloseTrace() will cause ProcessTrace() to
    // stop filling buffers and it will return after it finishes processing
    // events already in it's buffers.
    //
    // If collecting from a log file, ProcessTrace() will continue to process
    // the entire file though, which is why we cancel the processing from the
    // BufferCallback in this case.
    mContinueProcessingBuffers = FALSE;

    // Shutdown the trace and session.
    status = CloseTrace(mTraceHandle);
    mTraceHandle = INVALID_PROCESSTRACE_HANDLE;

    if (mSessionHandle != 0) {
        DisableProviders(mSessionHandle);

        TraceProperties sessionProps = {};
        sessionProps.Wnode.BufferSize = (ULONG) sizeof(TraceProperties);
        sessionProps.LoggerNameOffset = offsetof(TraceProperties, mSessionName);
        status = ControlTraceW(mSessionHandle, nullptr, &sessionProps, EVENT_TRACE_CONTROL_STOP);

        mSessionHandle = 0;
    }
}

ULONG TraceSession::StopNamedSession(char const* sessionName)
{
    TraceProperties sessionProps = {};
    sessionProps.Wnode.BufferSize = (ULONG) sizeof(TraceProperties);
    sessionProps.LoggerNameOffset = offsetof(TraceProperties, mSessionName);
    return ControlTraceA((TRACEHANDLE) 0, sessionName, &sessionProps, EVENT_TRACE_CONTROL_STOP);
}


ULONG TraceSession::CheckLostReports(ULONG* eventsLost, ULONG* buffersLost) const
{
    TraceProperties sessionProps = {};
    sessionProps.Wnode.BufferSize = (ULONG) sizeof(TraceProperties);
    sessionProps.LoggerNameOffset = offsetof(TraceProperties, mSessionName);

    auto status = ControlTraceW(mSessionHandle, nullptr, &sessionProps, EVENT_TRACE_CONTROL_QUERY);
    *eventsLost = sessionProps.EventsLost;
    *buffersLost = sessionProps.RealTimeBuffersLost;
    return status;
}

