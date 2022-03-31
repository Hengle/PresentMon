// Copyright (C) 2020-2021 Intel Corporation
// SPDX-License-Identifier: MIT

#define NOMINMAX
#include <gtest/gtest.h>
#include <string>
#include <unordered_map>
#include <windows.h>

struct PresentMonCsv
{
    enum Header {
        // Required headers:
        Header_Application,
        Header_ProcessID,
        Header_SwapChainAddress,
        Header_Runtime,
        Header_SyncInterval,
        Header_PresentFlags,
        Header_Dropped,
        Header_TimeInSeconds,
        Header_msBetweenPresents,
        Header_msInPresentAPI,

        // Optional headers:
        Header_QPCTime,
        Header_INTC_FrameID,
        Header_INTC_AppWorkStart,
        Header_INTC_AppSimulationTime,
        Header_INTC_DriverWorkStart,
        Header_INTC_DriverWorkEnd,
        Header_INTC_KernelDriverSubmitStart,
        Header_INTC_KernelDriverSubmitEnd,
        Header_INTC_GPUStart,
        Header_INTC_GPUEnd,
        Header_INTC_KernelDriverFenceReport,
        Header_INTC_PresentAPICall,
        Header_INTC_ScheduledFlipTime,
        Header_INTC_FlipReceivedTime,
        Header_INTC_FlipReportTime,
        Header_INTC_FlipProgrammingTime,
        Header_INTC_ActualFlipTime,

        // Required headers when -track_display is used:
        Header_AllowsTearing,
        Header_PresentMode,
        Header_msBetweenDisplayChange,
        Header_msUntilRenderComplete,
        Header_msUntilDisplayed,

        // Required headers when -track_debug is used:
        Header_WasBatched,
        Header_DwmNotified,

        // Required headers when -track_gpu is used:
        Header_msUntilRenderStart,
        Header_msGPUActive,

        // Required headers when -track_gpu_video is used:
        Header_msGPUVideoActive,

        // Required headers when -track_input is used:
        Header_msSinceInput,

        // Required headers when -track_queue_timers is used:
        Header_WaitIfFullTime,
        Header_WaitUntilEmptySyncTime,
        Header_WaitUntilEmptySyncAsincTime,
        Header_WaitUntilEmptyDrainTime,
        Header_WaitUntilEmptyDrainAsyncTime,
        Header_WaitForFence,
        Header_WaitUntilFenceSubmitted,
        Header_WaitIfEmptyTime,
        Header_FrameTimeApp,
        Header_FrameTimeDrv,

        // Required headers when -track_cpu_gpu_sync is used:
        Header_WaitSyncObjFromCpu,
        Header_WaitSyncObjFromGpu,
        Header_PollOnQueryGetData,

        // Special values:
        KnownHeaderCount,

        UnknownHeader,

        RequiredHeaderCount = 10,
        DisplayHeaderCount  = 5,
        DebugHeaderCount    = 2,
        GPUHeaderCount      = 2,
        GPUVideoHeaderCount = 1,
        InputHeaderCount    = 1,
        QueueTimersHeaderCount = 10,
        CpuGpuSyncHeaderCount  = 3,
    };

    static constexpr char const* GetHeaderString(Header h)
    {
        switch (h) {
        case Header_Application:            return "Application";
        case Header_ProcessID:              return "ProcessID";
        case Header_SwapChainAddress:       return "SwapChainAddress";
        case Header_Runtime:                return "Runtime";
        case Header_SyncInterval:           return "SyncInterval";
        case Header_PresentFlags:           return "PresentFlags";
        case Header_Dropped:                return "Dropped";
        case Header_TimeInSeconds:          return "TimeInSeconds";
        case Header_msBetweenPresents:      return "msBetweenPresents";
        case Header_msInPresentAPI:         return "msInPresentAPI";
        case Header_QPCTime:                return "QPCTime";
        case Header_INTC_FrameID:                 return "INTC_FrameID";
        case Header_INTC_AppWorkStart:            return "INTC_AppWorkStart";
        case Header_INTC_AppSimulationTime:       return "INTC_AppSimulationTime";
        case Header_INTC_DriverWorkStart:         return "INTC_DriverWorkStart";
        case Header_INTC_DriverWorkEnd:           return "INTC_DriverWorkEnd";
        case Header_INTC_KernelDriverSubmitStart: return "INTC_KernelDriverSubmitStart";
        case Header_INTC_KernelDriverSubmitEnd:   return "INTC_KernelDriverSubmitEnd";
        case Header_INTC_GPUStart:                return "INTC_GPUStart";
        case Header_INTC_GPUEnd:                  return "INTC_GPUEnd";
        case Header_INTC_KernelDriverFenceReport: return "INTC_KernelDriverFenceReport";
        case Header_INTC_PresentAPICall:          return "INTC_PresentAPICall";
        case Header_INTC_ScheduledFlipTime:       return "INTC_ScheduledFlipTime";
        case Header_INTC_FlipReceivedTime:        return "INTC_FlipReceivedTime";
        case Header_INTC_FlipReportTime:          return "INTC_FlipReportTime";
        case Header_INTC_FlipProgrammingTime:     return "INTC_FlipProgrammingTime";
        case Header_INTC_ActualFlipTime:          return "INTC_ActualFlipTime";
        case Header_AllowsTearing:          return "AllowsTearing";
        case Header_PresentMode:            return "PresentMode";
        case Header_msBetweenDisplayChange: return "msBetweenDisplayChange";
        case Header_msUntilRenderComplete:  return "msUntilRenderComplete";
        case Header_msUntilDisplayed:       return "msUntilDisplayed";
        case Header_WasBatched:             return "WasBatched";
        case Header_DwmNotified:            return "DwmNotified";
        case Header_msUntilRenderStart:     return "msUntilRenderStart";
        case Header_msGPUActive:            return "msGPUActive";
        case Header_msGPUVideoActive:       return "msGPUVideoActive";
        case Header_msSinceInput:           return "msSinceInput";
        case Header_WaitIfFullTime:               return "WaitIfFullTime";
        case Header_WaitUntilEmptySyncTime:       return "WaitUntilEmptySyncTime";
        case Header_WaitUntilEmptySyncAsincTime:  return "WaitUntilEmptySyncAsincTime";
        case Header_WaitUntilEmptyDrainTime:      return "WaitUntilEmptyDrainTime";
        case Header_WaitUntilEmptyDrainAsyncTime: return "WaitUntilEmptyDrainAsyncTime";
        case Header_WaitForFence:                 return "WaitForFence";
        case Header_WaitUntilFenceSubmitted:      return "WaitUntilFenceSubmitted";
        case Header_WaitIfEmptyTime:              return "WaitIfEmptyTime";
        case Header_FrameTimeApp:                 return "FrameTimeApp";
        case Header_FrameTimeDrv:                 return "FrameTimeDrv";
        case Header_WaitSyncObjFromCpu:           return "WaitSyncObjFromCpu";
        case Header_WaitSyncObjFromGpu:           return "WaitSyncObjFromGpu";
        case Header_PollOnQueryGetData:           return "PollOnQueryGetData";
        }
        return "<unknown>";
    }

    std::wstring path_;
    size_t line_;
    FILE* fp_;

    // headerColumnIndex_[h] is the file column index where h was found, or SIZE_MAX if
    // h wasn't found in the file.
    size_t headerColumnIndex_[KnownHeaderCount];

    char row_[1024];
    std::vector<char const*> cols_;
    bool trackDisplay_;
    bool trackDebug_;
    bool trackGPU_;
    bool trackGPUVideo_;
    bool trackInput_;
    bool trackQueueTimers_;
    bool trackCpuGpuSync_;

    PresentMonCsv();
    bool Open(char const* file, int line, std::wstring const& path);
    void Close();
    bool ReadRow();

    size_t GetColumnIndex(char const* header) const;
};

#define CSVOPEN(_P) Open(__FILE__, __LINE__, _P)

struct PresentMon : PROCESS_INFORMATION {
    static std::wstring exePath_;
    std::wstring cmdline_;
    bool csvArgSet_;

    PresentMon();
    ~PresentMon();

    void AddEtlPath(std::wstring const& etlPath);
    void AddCsvPath(std::wstring const& csvPath);
    void Add(wchar_t const* args);
    void Start(char const* file, int line);

    // Returns true if the process is still running for timeoutMilliseconds
    bool IsRunning(DWORD timeoutMilliseconds=0) const;

    // Expect the process to exit with expectedExitCode within
    // timeoutMilliseconds (or kill it otherwise).
    void ExpectExited(char const* file, int line, DWORD timeoutMilliseconds=INFINITE, DWORD expectedExitCode=0);
};

#define PMSTART() Start(__FILE__, __LINE__)
#define PMEXITED(...) ExpectExited(__FILE__, __LINE__, __VA_ARGS__)

extern std::wstring outDir_;

// PresentMon.cpp
void AddTestFailure(char const* file, int line, char const* fmt, ...);

// PresentMonTests.cpp
bool EnsureDirectoryCreated(std::wstring path);
std::string Convert(std::wstring const& s);
std::wstring Convert(std::string const& s);

// GoldEtlCsvTests.cpp
void AddGoldEtlCsvTests(std::wstring const& dir, size_t relIdx, bool reportAllCsvDiffs);
