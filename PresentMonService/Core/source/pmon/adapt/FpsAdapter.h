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
    class FpsAdapter
    {
    public:
        using Struct = PM_FPS_DATA;
        FpsAdapter(const PresentMon* pPmon);
        const Struct& Poll(double timestamp);
        const std::wstring category = L"FPS";
        void ClearCache();
    private:
        const PresentMon* pPmon = nullptr;
        bool swapChainWarningFired = false;
        std::optional<double> cacheTimestamp;
        Struct cache{};
    };

    template<> struct AdapterLookup<FpsAdapter::Struct> { using Adapter = FpsAdapter; };
}