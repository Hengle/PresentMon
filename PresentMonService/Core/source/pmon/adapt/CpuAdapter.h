// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <optional>
#include <PresentMonAPI/PresentMonAPI.h>
#include "AdapterLookup.h"

namespace p2c::pmon
{
    class PresentMon;
}

namespace p2c::pmon::adapt
{
    class CpuAdapter
    {
    public:
        using Struct = PM_CPU_DATA;
        CpuAdapter(const PresentMon* pPmon);
        const Struct& Poll(double timestamp);
        const std::wstring category = L"CPU";
        void ClearCache();
    private:
        const PresentMon* pPmon = nullptr;
        std::optional<double> cacheTimestamp;
        Struct cache{};
    };

    template<> struct AdapterLookup<CpuAdapter::Struct> { using Adapter = CpuAdapter; };
}