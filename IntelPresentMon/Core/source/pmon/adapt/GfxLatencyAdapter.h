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
    class GfxLatencyAdapter
    {
    public:
        using Struct = PM_GFX_LATENCY_DATA;
        GfxLatencyAdapter(const PresentMon* pPmon);
        const Struct& Poll(double timestamp);
        const std::wstring category = L"Gfx Latency";
        void ClearCache();
    private:
        const PresentMon* pPmon = nullptr;
        bool swapChainWarningFired = false;
        std::optional<double> cacheTimestamp;
        Struct cache{};
    };
    template<> struct AdapterLookup<GfxLatencyAdapter::Struct> { using Adapter = GfxLatencyAdapter; };
}