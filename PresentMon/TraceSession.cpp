// Copyright (C) 2017,2019-2022 Intel Corporation
// SPDX-License-Identifier: MIT

#include "PresentMon.hpp"

#include "../PresentData/TraceSession.hpp"
#include <VersionHelpers.h>

namespace {

TraceSession gSession;
static PMTraceConsumer* gPMConsumer = nullptr;
static MRTraceConsumer* gMRConsumer = nullptr;

bool FilteredEventsExpected()
{
    // Scope filtering based on event ID only works for realtime collection
    auto const& args = GetCommandLineArgs();
    if (args.mEtlFileName != nullptr) {
        return false;
    }

    // Scope filtering based on event ID doesn't work <Win8.1
    //
    // Note: IsWindows8Point1OrGreater() returns FALSE if the application isn't
    // built with a manifest.
    auto hmodule = LoadLibraryExA("ntdll.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (hmodule == NULL) {
        return false;
    }

    auto pRtlGetVersion = (NTSTATUS (WINAPI*)(PRTL_OSVERSIONINFOW)) GetProcAddress(hmodule, "RtlGetVersion");
    if (pRtlGetVersion == nullptr) {
        FreeLibrary(hmodule);
        return false;
    }

    RTL_OSVERSIONINFOW info = {};
    info.dwOSVersionInfoSize = sizeof(info);
    auto status = (*pRtlGetVersion)(&info);
    FreeLibrary(hmodule);

    // Win8.1 is version 6.3
    return status == 0 && (info.dwMajorVersion > 6 || (info.dwMajorVersion == 6 && info.dwMinorVersion >= 3));
}

}

bool StartTraceSession()
{
    auto const& args = GetCommandLineArgs();
    auto filterProcessIds = args.mTargetPid != 0; // Does not support process names at this point

    // Create consumers
    gPMConsumer = new PMTraceConsumer();
    gPMConsumer->mFilteredEvents = FilteredEventsExpected();
    gPMConsumer->mFilteredProcessIds = filterProcessIds;
    gPMConsumer->mTrackDisplay = args.mTrackDisplay;
    gPMConsumer->mTrackGPU = args.mTrackGPU;
    gPMConsumer->mTrackGPUVideo = args.mTrackGPUVideo;
    gPMConsumer->mTrackInput = args.mTrackInput;
    gPMConsumer->mTrackINTCQueueTimers = args.mTrackINTCQueueTimers;
    gPMConsumer->mTrackINTCCpuGpuSync = args.mTrackINTCCpuGpuSync;
    gPMConsumer->mDebugINTCFramePacing = args.mDebugINTCFramePacing;
    gPMConsumer->mTrackPCAT = args.mTrackPower;
	gPMConsumer->mTrackMemoryResidency = args.mTrackMemoryResidency;

    if (filterProcessIds) {
        gPMConsumer->AddTrackedProcessForFiltering(args.mTargetPid);
    }

    if (args.mTrackWMR) {
        gMRConsumer = new MRTraceConsumer(args.mTrackDisplay);
    }

    // Start the session;
    // If a session with this same name is already running, we either exit or
    // stop it and start a new session.  This is useful if a previous process
    // failed to properly shut down the session for some reason.
    auto status = gSession.Start(gPMConsumer, gMRConsumer, args.mEtlFileName, args.mSessionName);

    if (status == ERROR_ALREADY_EXISTS) {
        if (args.mStopExistingSession) {
            PrintWarning(
                "warning: a trace session named \"%s\" is already running and it will be stopped.\n"
                "         Use -session_name with a different name to start a new session.\n",
                args.mSessionName);
        } else {
            PrintError(
                "error: a trace session named \"%s\" is already running. Use -stop_existing_session\n"
                "       to stop the existing session, or use -session_name with a different name to\n"
                "       start a new session.\n",
                args.mSessionName);
            delete gPMConsumer;
            delete gMRConsumer;
            gPMConsumer = nullptr;
            gMRConsumer = nullptr;
            return false;
        }

        status = TraceSession::StopNamedSession(args.mSessionName);
        if (status == ERROR_SUCCESS) {
            status = gSession.Start(gPMConsumer, gMRConsumer, args.mEtlFileName, args.mSessionName);
        }
    }

    // Report error if we failed to start a new session
    if (status != ERROR_SUCCESS) {
        PrintError("error: failed to start trace session");
        switch (status) {
        case ERROR_FILE_NOT_FOUND:    PrintError(" (file not found)"); break;
        case ERROR_PATH_NOT_FOUND:    PrintError(" (path not found)"); break;
        case ERROR_BAD_PATHNAME:      PrintError(" (invalid --session_name)"); break;
        case ERROR_ACCESS_DENIED:     PrintError(" (access denied)"); break;
        case ERROR_FILE_CORRUPT:      PrintError(" (invalid --etl_file)"); break;
        default:                      PrintError(" (error=%lu)", status); break;
        }
        PrintError(".\n");

        if (status == ERROR_ACCESS_DENIED && !InPerfLogUsersGroup()) {
            PrintError(
                "       PresentMon requires either administrative privileges or to be run by a user in the\n"
                "       \"Performance Log Users\" user group.  View the readme for more details.\n");
        }

        delete gPMConsumer;
        delete gMRConsumer;
        gPMConsumer = nullptr;
        gMRConsumer = nullptr;
        return false;
    }

    // -------------------------------------------------------------------------
    // Start the consumer and output threads
    StartConsumerThread(gSession.mTraceHandle);
    StartOutputThread();

    return true;
}

void StopTraceSession()
{
    // Stop the trace session.
    gSession.Stop();

    // Wait for the consumer and output threads to end (which are using the
    // consumers).
    WaitForConsumerThreadToExit();
    StopOutputThread();

    // Destruct the consumers
    delete gMRConsumer;
    delete gPMConsumer;
    gMRConsumer = nullptr;
    gPMConsumer = nullptr;
}

void CheckLostReports(ULONG* eventsLost, ULONG* buffersLost)
{
    auto status = gSession.CheckLostReports(eventsLost, buffersLost);
    (void) status;
}

void DequeueAnalyzedInfo(
    std::vector<ProcessEvent>* processEvents,
    std::vector<std::shared_ptr<PresentEvent>>* presentEvents,
    std::vector<std::shared_ptr<PresentEvent>>* lostPresentEvents,
    std::vector<std::shared_ptr<LateStageReprojectionEvent>>* lsrs)
{
    gPMConsumer->DequeueProcessEvents(*processEvents);
    gPMConsumer->DequeuePresentEvents(*presentEvents);
    gPMConsumer->DequeueLostPresentEvents(*lostPresentEvents);
    if (gMRConsumer != nullptr) {
        gMRConsumer->DequeueLSRs(*lsrs);
    }
}

double QpcDeltaToSeconds(uint64_t qpcDelta)
{
    return (double) qpcDelta / gSession.mQpcFrequency.QuadPart;
}

uint64_t SecondsDeltaToQpc(double secondsDelta)
{
    return (uint64_t) (secondsDelta * gSession.mQpcFrequency.QuadPart);
}

double QpcToSeconds(uint64_t qpc)
{
    return QpcDeltaToSeconds(qpc - gSession.mStartQpc.QuadPart);
}

void QpcToLocalSystemTime(uint64_t qpc, SYSTEMTIME* st, uint64_t* ns)
{
    auto tns = (qpc - gSession.mStartQpc.QuadPart) * 1000000000ull / gSession.mQpcFrequency.QuadPart;
    auto t100ns = tns / 100;
    auto ft = (*(uint64_t*) &gSession.mStartTime) + t100ns;

    FileTimeToSystemTime((FILETIME const*) &ft, st);
    *ns = (ft - (ft / 10000000ull) * 10000000ull) * 100ull + (tns - t100ns * 100ull);
}
