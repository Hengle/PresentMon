// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <optional>
#include <chrono>
#include "../AdapterInfo.h"

namespace p2c::pmon
{
    class PresentMon;
}

namespace p2c::pmon::adapt
{
    class InfoAdapter
    {
    public:
        const std::wstring category = L"Info";
        InfoAdapter(const pmon::PresentMon* pPmon);
        std::wstring GetElapsedTime() const;
        std::wstring GetGpuName() const;
        std::wstring GetCpuName() const;
        std::wstring GetDateTime() const;
        void CaptureState();
    private:
        const pmon::PresentMon* pPmon = nullptr;
        std::optional<std::vector<pmon::AdapterInfo>> adapters;
        mutable std::optional<std::wstring> cpuNameCache;
        std::optional<std::chrono::system_clock::time_point> startTime;
    };
}