// Copyright (C) 2019-2021 Intel Corporation
// SPDX-License-Identifier: MIT

#include "PresentMon.hpp"
#include "../PresentData/ETW/Intel_Graphics_D3D10.h"

static OutputCsv gSingleOutputCsv = {};
static uint32_t gRecordingCount = 1;

void IncrementRecordingCount()
{
    gRecordingCount += 1;
}

const char* PresentModeToString(PresentMode mode)
{
    switch (mode) {
    case PresentMode::Hardware_Legacy_Flip: return "Hardware: Legacy Flip";
    case PresentMode::Hardware_Legacy_Copy_To_Front_Buffer: return "Hardware: Legacy Copy to front buffer";
    case PresentMode::Hardware_Independent_Flip: return "Hardware: Independent Flip";
    case PresentMode::Composed_Flip: return "Composed: Flip";
    case PresentMode::Composed_Copy_GPU_GDI: return "Composed: Copy with GPU GDI";
    case PresentMode::Composed_Copy_CPU_GDI: return "Composed: Copy with CPU GDI";
    case PresentMode::Hardware_Composed_Independent_Flip: return "Hardware Composed: Independent Flip";
    default: return "Other";
    }
}

const char* RuntimeToString(Runtime rt)
{
    switch (rt) {
    case Runtime::DXGI: return "DXGI";
    case Runtime::D3D9: return "D3D9";
    case Runtime::CloudStreaming: return "CloudStreaming";
    default: return "Other";
    }
}

const char* FinalStateToDroppedString(PresentResult res)
{
    switch (res) {
    case PresentResult::Presented: return "0";
    default: return "1";
    }
}

static void WriteCsvHeader(FILE* fp)
{
    auto const& args = GetCommandLineArgs();

    fprintf(fp,
        "Application"
        ",ProcessID"
        ",SwapChainAddress"
        ",Runtime"
        ",SyncInterval"
        ",PresentFlags"
        ",Dropped"
        ",%s"
        ",msInPresentAPI"
        ",msBetweenPresents",
        args.mOutputDateTime ? "PresentTime" : "TimeInSeconds");
    if (args.mTrackDisplay) {
        fprintf(fp,
            ",AllowsTearing"
            ",PresentMode"
            ",msUntilRenderComplete"
            ",msUntilDisplayed"
            ",msBetweenDisplayChange");
    }
    if (args.mTrackDebug) {
        fprintf(fp,
            ",WasBatched"
            ",DwmNotified");
    }
    if (args.mTrackGPU) {
        fprintf(fp,
            ",msUntilRenderStart"
            ",msGPUActive");
    }
    if (args.mTrackGPUVideo) {
        fprintf(fp, ",msGPUVideoActive");
    }
    if (args.mTrackInput) {
        fprintf(fp, ",msSinceInput");
    }
    if (args.mTrackINTCTimers) {
        fprintf(fp,
            ",msStalledOnQueueFull"
            ",msWaitingOnQueueSync"
            ",msWaitingOnQueueDrain"
            ",msWaitingOnFence"
            ",msWaitingOnFenceSubmission"
            ",msStalledOnQueueEmpty"
            ",msBetweenProducerPresents"
            ",msBetweenConsumerPresents");
    }
    if (args.mTrackINTCCpuGpuSync) {
        fprintf(fp,
            ",msWaitingOnSyncObject"
            ",msWaitingOnQueryData");
    }
    if (args.mTrackINTCShaderCompilation) {
        fprintf(fp,
            ",msWaitingOnDrawTimeCompilation"
            ",msWaitingOnCreateTimeCompilation");
    }
    if (args.mTrackMemoryResidency) {
        fprintf(fp,
            ",msInMakeResident"
            ",msInPagingPackets");
    }
    if (args.mOutputQpcTime) {
        fprintf(fp, ",QPCTime");
    }
    if (args.mDebugINTCFramePacing) {
        fprintf(fp,
            ",INTC_FrameID"
            ",INTC_AppWorkStart"
            ",INTC_AppSimulationTime"
            ",INTC_DriverWorkStart"
            ",INTC_DriverWorkEnd"
            ",INTC_KernelDriverSubmitStart"
            ",INTC_KernelDriverSubmitEnd"
            ",INTC_GPUStart"
            ",INTC_GPUEnd"
            ",INTC_KernelDriverFenceReport"
            ",INTC_PresentAPICall"
            ",INTC_ScheduledFlipTime"
            ",INTC_FlipReceivedTime"
            ",INTC_FlipReportTime"
            ",INTC_FlipProgrammingTime"
            ",INTC_ActualFlipTime");
    }
    fprintf(fp, "\n");
}

void UpdateCsv(ProcessInfo* processInfo, SwapChainData const& chain, PresentEvent* pp)
{
    auto const& args = GetCommandLineArgs();
    auto const& p = *pp;

    // Don't output dropped frames (if requested).
    auto presented = p.FinalState == PresentResult::Presented;
    if (args.mExcludeDropped && !presented) {
        return;
    }

    // Early return if not outputing to CSV.
    auto fp = GetOutputCsv(processInfo).mFile;
    if (fp == nullptr) {
        return;
    }

    // Look up the last present event in the swapchain's history.  We need at
    // least two presents to compute frame statistics.
    if (chain.mPresentHistoryCount == 0) {
        return;
    }

    auto lastPresented = chain.mPresentHistory[(chain.mNextPresentIndex - 1) % SwapChainData::PRESENT_HISTORY_MAX_COUNT].get();

    // Compute frame statistics.
    double msBetweenPresents      = 1000.0 * QpcDeltaToSeconds(p.QpcTime - lastPresented->QpcTime);
    double msInPresentApi         = 1000.0 * QpcDeltaToSeconds(p.TimeTaken);
    double msUntilRenderStart     = 0.0;
    double msUntilRenderComplete  = 0.0;
    double msUntilDisplayed       = 0.0;
    double msBetweenDisplayChange = 0.0;
    double msSinceInput           = 0.0;

    if (args.mTrackDisplay) {
        if (p.ReadyTime != 0) {
            if (p.ReadyTime < p.QpcTime) {
                msUntilRenderComplete = -1000.0 * QpcDeltaToSeconds(p.QpcTime - p.ReadyTime);
            } else {
                msUntilRenderComplete = 1000.0 * QpcDeltaToSeconds(p.ReadyTime - p.QpcTime);
            }
        }
        if (presented) {
            msUntilDisplayed = 1000.0 * QpcDeltaToSeconds(p.ScreenTime - p.QpcTime);

            if (chain.mLastDisplayedPresentIndex > 0) {
                auto lastDisplayed = chain.mPresentHistory[chain.mLastDisplayedPresentIndex % SwapChainData::PRESENT_HISTORY_MAX_COUNT].get();
                msBetweenDisplayChange = 1000.0 * QpcDeltaToSeconds(p.ScreenTime - lastDisplayed->ScreenTime);
            }
        }
    }

    if (args.mTrackGPU) {
        if (p.GPUStartTime != 0) {
            if (p.GPUStartTime < p.QpcTime) {
                msUntilRenderStart = -1000.0 * QpcDeltaToSeconds(p.QpcTime - p.GPUStartTime);
            } else {
                msUntilRenderStart = 1000.0 * QpcDeltaToSeconds(p.GPUStartTime - p.QpcTime);
            }
        }
    }

    if (args.mTrackInput) {
        if (p.InputTime != 0) {
            msSinceInput = 1000.0 * QpcDeltaToSeconds(p.QpcTime - p.InputTime);
        }
    }

    double msBetweenProducerPresents = 0.0;
    double msBetweenConsumerPresents = 0.0;
    if (args.mTrackINTCTimers) {
        if (p.INTC_ProducerPresentTime != 0 && p.INTC_ProducerPresentTime > lastPresented->INTC_ProducerPresentTime) {
            msBetweenProducerPresents = 1000.0 * QpcDeltaToSeconds(p.INTC_ProducerPresentTime - lastPresented->INTC_ProducerPresentTime);
        }
        if (p.INTC_ConsumerPresentTime != 0 && p.INTC_ConsumerPresentTime > lastPresented->INTC_ConsumerPresentTime) {
            msBetweenConsumerPresents = 1000.0 * QpcDeltaToSeconds(p.INTC_ConsumerPresentTime - lastPresented->INTC_ConsumerPresentTime);
        }
    }

    // Temporary calculation while debugging.  Allow timestamps to be before or after QpcTime for now.
    double INTC_AppWorkStart = 0.0;
    double INTC_AppSimulationTime = 0.0;
    double INTC_DriverWorkStart = 0.0;
    double INTC_DriverWorkEnd = 0.0;
    double INTC_KernelDriverSubmitStart = 0.0;
    double INTC_KernelDriverSubmitEnd = 0.0;
    double INTC_GPUStart = 0.0;
    double INTC_GPUEnd = 0.0;
    double INTC_KernelDriverFenceReport = 0.0;
    double INTC_PresentAPICall = 0.0;
    double INTC_FlipReceivedTime = 0.0;
    double INTC_FlipReportTime = 0.0;
    double INTC_FlipProgrammingTime = 0.0;
    double INTC_ActualFlipTime = 0.0;
    double INTC_ScheduledFlipTime = 0.0;
    if (args.mDebugINTCFramePacing) {
        #define INTC_CALC_DELTA(_N) _N = p._N == 0         ? 0.0 : \
                                         p._N >= p.QpcTime ? 1000.0 * QpcDeltaToSeconds(p._N - p.QpcTime) : \
                                                            -1000.0 * QpcDeltaToSeconds(p.QpcTime - p._N)
        INTC_CALC_DELTA(INTC_AppWorkStart);
        INTC_CALC_DELTA(INTC_AppSimulationTime);
        INTC_CALC_DELTA(INTC_DriverWorkStart);
        INTC_CALC_DELTA(INTC_DriverWorkEnd);
        INTC_CALC_DELTA(INTC_KernelDriverSubmitStart);
        INTC_CALC_DELTA(INTC_KernelDriverSubmitEnd);
        INTC_CALC_DELTA(INTC_GPUStart);
        INTC_CALC_DELTA(INTC_GPUEnd);
        INTC_CALC_DELTA(INTC_KernelDriverFenceReport);
        INTC_CALC_DELTA(INTC_PresentAPICall);
        INTC_CALC_DELTA(INTC_FlipReceivedTime);
        INTC_CALC_DELTA(INTC_FlipReportTime);
        INTC_CALC_DELTA(INTC_FlipProgrammingTime);
        INTC_CALC_DELTA(INTC_ActualFlipTime);
        #undef INTC_CALC_DELTA

        // ScheduledFlipTime[N] = max(ScheduledFlipTime[N-1], FlipReceivedTime[N-1]) + TargetFrameTime[N]
        if (p.INTC_TargetFrameTime != 0 && chain.mLastDisplayedPresentIndex > 0) {
            auto lastDisplayed = chain.mPresentHistory[chain.mLastDisplayedPresentIndex % SwapChainData::PRESENT_HISTORY_MAX_COUNT].get();

            // NOTE: once ScheduledFlipTime is computed for a particular present,
            // we store it by overwriting p.INTC_TargetFrameTime.
            auto scheduledQpc = max(lastDisplayed->INTC_TargetFrameTime, lastDisplayed->INTC_FlipReceivedTime) + pp->INTC_TargetFrameTime;
            pp->INTC_TargetFrameTime = scheduledQpc;

            INTC_ScheduledFlipTime = scheduledQpc >= p.QpcTime ? 1000.0 * QpcDeltaToSeconds(scheduledQpc - p.QpcTime) :
                                                                -1000.0 * QpcDeltaToSeconds(p.QpcTime - scheduledQpc);
        }
    }

    // Output in CSV format
    fprintf(fp, "%s,%d,0x%016llX,%s,%d,%d,%s,",
        processInfo->mModuleName.c_str(),
        p.ProcessId,
        p.SwapChainAddress,
        RuntimeToString(p.Runtime),
        p.SyncInterval,
        p.PresentFlags,
        FinalStateToDroppedString(p.FinalState));
    if (args.mOutputDateTime) {
        SYSTEMTIME st = {};
        uint64_t ns = 0;
        QpcToLocalSystemTime(p.QpcTime, &st, &ns);
        fprintf(fp, "%u-%u-%u %u:%02u:%02u.%09llu",
            st.wYear,
            st.wMonth,
            st.wDay,
            st.wHour,
            st.wMinute,
            st.wSecond,
            ns);
    } else {
        fprintf(fp, "%.*lf", DBL_DIG - 1, QpcToSeconds(p.QpcTime));
    }
    fprintf(fp, ",%.*lf,%.*lf",
        DBL_DIG - 1, msInPresentApi,
        DBL_DIG - 1, msBetweenPresents);
    if (args.mTrackDisplay) {
        fprintf(fp, ",%d,%s,%.*lf,%.*lf,%.*lf",
            p.SupportsTearing,
            PresentModeToString(p.PresentMode),
            DBL_DIG - 1, msUntilRenderComplete,
            DBL_DIG - 1, msUntilDisplayed,
            DBL_DIG - 1, msBetweenDisplayChange);
    }
    if (args.mTrackDebug) {
        fprintf(fp, ",%d,%d",
            p.DriverBatchThreadId != 0,
            p.DwmNotified);
    }
    if (args.mTrackGPU) {
        fprintf(fp, ",%.*lf,%.*lf",
            DBL_DIG - 1, msUntilRenderStart,
            DBL_DIG - 1, 1000.0 * QpcDeltaToSeconds(p.GPUDuration));
    }
    if (args.mTrackGPUVideo) {
        fprintf(fp, ",%.*lf",
            DBL_DIG - 1, 1000.0 * QpcDeltaToSeconds(p.GPUVideoDuration));
    }
    if (args.mTrackInput) {
        fprintf(fp, ",%.*lf", DBL_DIG - 1, msSinceInput);
    }
    if (args.mTrackINTCTimers) {
        fprintf(fp, ",%.*lf,%.*lf,%.*lf,%.*lf,%.*lf,%.*lf,%.*lf,%.*lf",
            DBL_DIG - 1, 1000.0 * QpcDeltaToSeconds(p.INTC_Timer[INTC_TIMER_WAIT_IF_FULL].mAccumulatedTime),
            DBL_DIG - 1, 1000.0 * QpcDeltaToSeconds(p.INTC_Timer[INTC_TIMER_WAIT_UNTIL_EMPTY_SYNC].mAccumulatedTime),
            DBL_DIG - 1, 1000.0 * QpcDeltaToSeconds(p.INTC_Timer[INTC_TIMER_WAIT_UNTIL_EMPTY_DRAIN].mAccumulatedTime),
            DBL_DIG - 1, 1000.0 * QpcDeltaToSeconds(p.INTC_Timer[INTC_TIMER_WAIT_FOR_FENCE].mAccumulatedTime),
            DBL_DIG - 1, 1000.0 * QpcDeltaToSeconds(p.INTC_Timer[INTC_TIMER_WAIT_UNTIL_FENCE_SUBMITTED].mAccumulatedTime),
            DBL_DIG - 1, 1000.0 * QpcDeltaToSeconds(p.INTC_Timer[INTC_TIMER_WAIT_IF_EMPTY].mAccumulatedTime),
            DBL_DIG - 1, msBetweenProducerPresents,
            DBL_DIG - 1, msBetweenConsumerPresents);
    }
    if (args.mTrackINTCCpuGpuSync) {
        fprintf(fp, ",%.*lf,%.*lf",
            DBL_DIG -1, 1000.0 * QpcDeltaToSeconds(p.INTC_Timers[INTC_GPU_TIMER_SYNC_TYPE_WAIT_SYNC_OBJECT_CPU]),
            DBL_DIG -1, 1000.0 * QpcDeltaToSeconds(p.INTC_Timers[INTC_GPU_TIMER_SYNC_TYPE_POLL_ON_QUERY_GET_DATA]));
    }
    if (args.mTrackINTCShaderCompilation) {
        fprintf(fp, ",%.*lf,%.*lf",
            DBL_DIG - 1, 1000.0 * QpcDeltaToSeconds(p.INTC_Timers[INTC_GPU_TIMER_WAIT_FOR_COMPILATION_ON_DRAW]),
            DBL_DIG - 1, 1000.0 * QpcDeltaToSeconds(p.INTC_Timers[INTC_GPU_TIMER_WAIT_FOR_COMPILATION_ON_CREATE]));
    }
    if (args.mTrackMemoryResidency) {
        fprintf(fp, ",%.*lf,%.*lf",
            DBL_DIG - 1, 1000.0 * QpcDeltaToSeconds(p.MemoryResidency[DXGK_RESIDENCY_EVENT_MAKE_RESIDENT]),
            DBL_DIG - 1, 1000.0 * QpcDeltaToSeconds(p.MemoryResidency[DXGK_RESIDENCY_EVENT_PAGING_QUEUE_PACKET]));
    }
    if (args.mOutputQpcTime) {
        if (args.mOutputQpcTimeInSeconds) {
            fprintf(fp, ",%.*lf", DBL_DIG - 1, QpcDeltaToSeconds(p.QpcTime));
        } else {
            fprintf(fp, ",%llu", p.QpcTime);
        }
    }
    if (args.mDebugINTCFramePacing) {
        fprintf(fp, ",%llu", p.INTC_FrameID);
        fprintf(fp, ",%lf", INTC_AppWorkStart);
        fprintf(fp, ",%lf", INTC_AppSimulationTime);
        fprintf(fp, ",%lf", INTC_DriverWorkStart);
        fprintf(fp, ",%lf", INTC_DriverWorkEnd);
        fprintf(fp, ",%lf", INTC_KernelDriverSubmitStart);
        fprintf(fp, ",%lf", INTC_KernelDriverSubmitEnd);
        fprintf(fp, ",%lf", INTC_GPUStart);
        fprintf(fp, ",%lf", INTC_GPUEnd);
        fprintf(fp, ",%lf", INTC_KernelDriverFenceReport);
        fprintf(fp, ",%lf", INTC_PresentAPICall);
        fprintf(fp, ",%lf", INTC_ScheduledFlipTime);
        fprintf(fp, ",%lf", INTC_FlipReceivedTime);
        fprintf(fp, ",%lf", INTC_FlipReportTime);
        fprintf(fp, ",%lf", INTC_FlipProgrammingTime);
        fprintf(fp, ",%lf", INTC_ActualFlipTime);
    }
    fprintf(fp, "\n");
}

/* This text is reproduced in the readme, modify both if there are changes:

By default, PresentMon creates a CSV file named `PresentMon-TIME.csv`, where
`TIME` is the creation time in ISO 8601 format.  To specify your own output
location, use the `-output_file PATH` command line argument.

If `-multi_csv` is used, then one CSV is created for each process captured with
`-PROCESSNAME` appended to the file name.

If `-hotkey` is used, then one CSV is created each time recording is started
with `-INDEX` appended to the file name.

If `-include_mixed_reality` is used, a second CSV file will be generated with
`_WMR` appended to the filename containing the WMR data.
*/
static void GenerateFilename(char const* processName, char* path)
{
    auto const& args = GetCommandLineArgs();

    char ext[_MAX_EXT];
    int pathLength = MAX_PATH;

#define ADD_TO_PATH(...) do { \
    if (path != nullptr) { \
        auto result = _snprintf_s(path, pathLength, _TRUNCATE, __VA_ARGS__); \
        if (result == -1) path = nullptr; else { path += result; pathLength -= result; } \
    } \
} while (0)

    // Generate base filename.
    if (args.mOutputCsvFileName) {
        char drive[_MAX_DRIVE];
        char dir[_MAX_DIR];
        char name[_MAX_FNAME];
        _splitpath_s(args.mOutputCsvFileName, drive, dir, name, ext);
        ADD_TO_PATH("%s%s%s", drive, dir, name);
    } else {
        struct tm tm;
        time_t time_now = time(NULL);
        localtime_s(&tm, &time_now);
        ADD_TO_PATH("PresentMon-%4d-%02d-%02dT%02d%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        strcpy_s(ext, ".csv");
    }

    // Append -PROCESSNAME if applicable.
    if (processName != nullptr) {
        ADD_TO_PATH("-%s", processName);
    }

    // Append -INDEX if applicable.
    if (args.mHotkeySupport) {
        ADD_TO_PATH("-%d", gRecordingCount);
    }

    // Append extension.
    ADD_TO_PATH("%s", ext);
}

static OutputCsv CreateOutputCsv(char const* processName)
{
    auto const& args = GetCommandLineArgs();

    OutputCsv outputCsv = {};

    if (args.mOutputCsvToStdout) {
        outputCsv.mFile = stdout;
        outputCsv.mWmrFile = nullptr;       // WMR disallowed if -output_stdout
    } else {
        char path[MAX_PATH];
        GenerateFilename(processName, path);

        fopen_s(&outputCsv.mFile, path, "w");

        if (args.mTrackWMR) {
            outputCsv.mWmrFile = CreateLsrCsvFile(path);
        }
    }

    if (outputCsv.mFile != nullptr) {
        WriteCsvHeader(outputCsv.mFile);
    }

    return outputCsv;
}

OutputCsv GetOutputCsv(ProcessInfo* processInfo)
{
    auto const& args = GetCommandLineArgs();

    // TODO: If fopen_s() fails to open mFile, we'll just keep trying here
    // every time PresentMon wants to output to the file. We should detect the
    // failure and generate an error instead.

    if (args.mOutputCsvToFile && processInfo->mOutputCsv.mFile == nullptr) {
        if (args.mMultiCsv) {
            processInfo->mOutputCsv = CreateOutputCsv(processInfo->mModuleName.c_str());
        } else {
            if (gSingleOutputCsv.mFile == nullptr) {
                gSingleOutputCsv = CreateOutputCsv(nullptr);
            }

            processInfo->mOutputCsv = gSingleOutputCsv;
        }
    }

    return processInfo->mOutputCsv;
}

void CloseOutputCsv(ProcessInfo* processInfo)
{
    auto const& args = GetCommandLineArgs();

    // If processInfo is nullptr, it means we should operate on the global
    // single output CSV.
    //
    // We only actually close the FILE if we own it (we're operating on the
    // single global output CSV, or we're writing a CSV per process) and it's
    // not stdout.
    OutputCsv* csv = nullptr;
    bool closeFile = false;
    if (processInfo == nullptr) {
        csv = &gSingleOutputCsv;
        closeFile = !args.mOutputCsvToStdout;
    } else {
        csv = &processInfo->mOutputCsv;
        closeFile = !args.mOutputCsvToStdout && args.mMultiCsv;
    }

    if (closeFile) {
        if (csv->mFile != nullptr) {
            fclose(csv->mFile);
        }
        if (csv->mWmrFile != nullptr) {
            fclose(csv->mWmrFile);
        }
    }

    csv->mFile = nullptr;
    csv->mWmrFile = nullptr;
}

