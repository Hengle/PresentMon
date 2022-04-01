// Copyright (C) 2020-2022 Intel Corporation
// SPDX-License-Identifier: MIT

#include "PresentMonTests.h"

void AddTestFailure(char const* file, int line, char const* fmt, ...)
{
    char buffer[512];

    va_list val;
    va_start(val, fmt);
    vsnprintf(buffer, _countof(buffer), fmt, val);
    va_end(val);

    GTEST_MESSAGE_AT_(file, line, buffer, ::testing::TestPartResult::kNonFatalFailure);
}

namespace {

PresentMonCsv::Header FindHeader(char const* header)
{
    for (uint32_t i = 0; i < PresentMonCsv::KnownHeaderCount; ++i) {
        auto h = (PresentMonCsv::Header) i;
        if (strcmp(header, PresentMonCsv::GetHeaderString(h)) == 0) {
            return h;
        }
    }
    return PresentMonCsv::UnknownHeader;
}

}

PresentMonCsv::PresentMonCsv()
    : line_(0)
    , fp_(nullptr)
    , trackDisplay_(false)
    , trackDebug_(false)
    , trackGPU_(false)
    , trackGPUVideo_(false)
    , trackInput_(false)
    , trackQueueTimers_(false)
    , trackCpuGpuSync_(false)
{
}

bool PresentMonCsv::Open(char const* file, int line, std::wstring const& path)
{
    for (uint32_t i = 0; i < _countof(headerColumnIndex_); ++i) {
        headerColumnIndex_[i] = SIZE_MAX;
    }
    cols_.clear();
    path_ = path;
    line_ = 0;

    if (_wfopen_s(&fp_, path.c_str(), L"r")) {
        AddTestFailure(file, line, "Failed to open file: %ls", path.c_str());
        return false;
    }

    // Remove UTF-8 marker if there is one.
    long int startOfs = 0;
    if (fread(row_, 3, 1, fp_) == 1 &&
        row_[0] == -17 &&
        row_[1] == -69 &&
        row_[2] == -65) {
        startOfs = 3;
    }
    fseek(fp_, startOfs, SEEK_SET);

    // Read the header and ensure required columns are present
    ReadRow();

    uint32_t requiredCount = 0;
    uint32_t trackDisplayCount = 0;
    uint32_t trackDebugCount = 0;
    uint32_t trackGPUCount = 0;
    uint32_t trackGPUVideoCount = 0;
    uint32_t trackInputCount = 0;
    uint32_t trackQueueTimersCount = 0;
    uint32_t trackCpuGpuSyncCount = 0;
    for (size_t i = 0, n = cols_.size(); i < n; ++i) {
        auto h = FindHeader(cols_[i]);
        if (h == UnknownHeader) {
            AddTestFailure(Convert(path_).c_str(), (int) line_, "Unrecognised column: %s", cols_[i]);
        } else if (headerColumnIndex_[(size_t) h] != SIZE_MAX) {
            AddTestFailure(Convert(path_).c_str(), (int) line_, "Duplicate column: %s", cols_[i]);
        } else {
            headerColumnIndex_[(size_t) h] = i;

            switch (h) {
            case Header_Application:
            case Header_ProcessID:
            case Header_SwapChainAddress:
            case Header_Runtime:
            case Header_SyncInterval:
            case Header_PresentFlags:
            case Header_Dropped:
            case Header_TimeInSeconds:
            case Header_msBetweenPresents:
            case Header_msInPresentAPI:
                requiredCount += 1;
                break;

            case Header_AllowsTearing:
            case Header_PresentMode:
            case Header_msBetweenDisplayChange:
            case Header_msUntilRenderComplete:
            case Header_msUntilDisplayed:
                trackDisplayCount += 1;
                break;

            case Header_WasBatched:
            case Header_DwmNotified:
                trackDebugCount += 1;
                break;

            case Header_msUntilRenderStart:
            case Header_msGPUActive:
                trackGPUCount += 1;
                break;

            case Header_msGPUVideoActive:
                trackGPUVideoCount += 1;
                break;

            case Header_msSinceInput:
                trackInputCount += 1;
                break;

            case Header_msStalledOnFullQueue:
            case Header_msStalledOnEmptyQueue:
            case Header_msWaitingOnQueueSync:
            case Header_msWaitingOnQueueDrain:
            case Header_msWaitingOnFence:
            case Header_msWaitingOnFenceSubmission:
            case Header_ProducerPresentTime:
            case Header_ConsumerPresentTime:
                trackQueueTimersCount += 1;
                break;

            case Header_msWaitingOnSyncObject:
            case Header_msWaitingOnQueryData:
                trackCpuGpuSyncCount += 1;
                break;
            }
        }
    }

    trackDisplay_  = trackDisplayCount > 0;
    trackDebug_    = trackDebugCount > 0;
    trackGPU_      = trackGPUCount > 0;
    trackGPUVideo_ = trackGPUVideoCount > 0;
    trackInput_    = trackInputCount > 0;
    trackQueueTimers_ = trackQueueTimersCount > 0;
    trackCpuGpuSync_  = trackCpuGpuSyncCount > 0;

    if (                   requiredCount      != RequiredHeaderCount ||
        (trackDisplay_  && trackDisplayCount  != DisplayHeaderCount) ||
        (trackDebug_    && trackDebugCount    != DebugHeaderCount) ||
        (trackGPU_      && trackGPUCount      != GPUHeaderCount) ||
        (trackGPUVideo_ && trackGPUVideoCount != GPUVideoHeaderCount) ||
        (trackInput_    && trackInputCount    != InputHeaderCount) ||
        (trackQueueTimers_ && trackQueueTimersCount != QueueTimersHeaderCount) ||
        (trackCpuGpuSync_  && trackCpuGpuSyncCount  != CpuGpuSyncHeaderCount)) {
        AddTestFailure(Convert(path_).c_str(), (int) line_, "Missing required columns.");
    }

    return true;
}

void PresentMonCsv::Close()
{
    fclose(fp_);
    fp_ = nullptr;
}

bool PresentMonCsv::ReadRow()
{
    row_[0] = '\0';
    cols_.clear();

    // Read a line
    if (fgets(row_, _countof(row_), fp_) == nullptr) {
        if (ferror(fp_) != 0) {
            AddTestFailure(Convert(path_).c_str(), (int) line_, "File read error");
        }
        return false;
    }

    line_ += 1;

    // Split line into columns, skipping leading/trailing whitespace
    auto p0 = row_;
    for (; *p0 == ' ' || *p0 == '\t'; ++p0) *p0 = '\0';
    for (auto p = p0; ; ++p) {
        auto ch = *p;
        if (ch == ',' || ch == '\0') {
            *p = '\0';
            cols_.push_back(p0);
            for (p0 = p + 1; *p0 == ' ' || *p0 == '\t'; ++p0) *p0 = '\0';
            for (auto q = p - 1; *q == ' ' || *q == '\t' || *q == '\n' || *q == '\r'; --q) *q = '\0';
            if (ch == '\0') break;
        }
    }

    return true;
}

size_t PresentMonCsv::GetColumnIndex(char const* header) const
{
    auto h = FindHeader(header);
    return h == UnknownHeader ? SIZE_MAX : headerColumnIndex_[h];
}

PresentMon::PresentMon()
    : cmdline_()
    , csvArgSet_(false)
{
    cmdline_ += L'\"';
    cmdline_ += exePath_;
    cmdline_ += L"\" -no_top";
}

PresentMon::~PresentMon()
{
    if (::testing::Test::HasFailure()) {
        printf("%ls\n", cmdline_.c_str());
    }
}

void PresentMon::AddEtlPath(std::wstring const& etlPath)
{
    cmdline_ += L" -etl_file \"";
    cmdline_ += etlPath;
    cmdline_ += L'\"';
}

void PresentMon::AddCsvPath(std::wstring const& csvPath)
{
    EXPECT_FALSE(csvArgSet_);
    cmdline_ += L" -output_file \"";
    cmdline_ += csvPath;
    cmdline_ += L'\"';
    csvArgSet_ = true;

    // Delete the file if it exists.  Otherwise, PresentMon may not output
    // anything and any previous content will remain.
    DeleteFile(csvPath.c_str());
}

void PresentMon::Add(wchar_t const* args)
{
    cmdline_ += L' ';
    cmdline_ += args;
}

void PresentMon::Start(char const* file, int line)
{
    if (!csvArgSet_) {
        cmdline_ += L" -no_csv";
        csvArgSet_ = true;
    }

    STARTUPINFO si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    if (CreateProcess(nullptr, &cmdline_[0], nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, (PROCESS_INFORMATION*) this) == 0) {
        AddTestFailure(file, line, "Failed to start PresentMon");
    }
}

bool PresentMon::IsRunning(DWORD timeoutMilliseconds) const
{
    return WaitForSingleObject(hProcess, timeoutMilliseconds) == WAIT_TIMEOUT;
}

void PresentMon::ExpectExited(char const* file, int line, DWORD timeoutMilliseconds, DWORD expectedExitCode)
{
    auto isRunning = IsRunning(timeoutMilliseconds);
    if (isRunning) {
        AddTestFailure(file, line, "PresentMon still running after %ums", timeoutMilliseconds);

        TerminateProcess(hProcess, 0);
        WaitForSingleObject(hProcess, INFINITE);
    } else {
        DWORD exitCode = 0;
        GetExitCodeProcess(hProcess, &exitCode);

        if (exitCode != expectedExitCode) {
            AddTestFailure(file, line, "Unexpected PresentMon exit code: %d (expecting %d)", exitCode, expectedExitCode);
        }
    }

    CloseHandle(hProcess);
    CloseHandle(hThread);
}
