// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <optional>
#include <span>
#include <vector>
#include <PresentMonAPI/PresentMonAPI.h>

namespace p2c::pmon
{
    class PresentMon;
}

namespace p2c::pmon::adapt
{
    class RawAdapter
    {
    public:
        using Struct = PM_FRAME_DATA;
        RawAdapter(const PresentMon* pPmon);
        std::span<const Struct> Pull(double timestamp);
        void ClearCache();
    private:
        static constexpr size_t initialCacheSize = 120;
        const PresentMon* pPmon;
        // bool swapChainWarningFired = false;
        std::optional<double> cacheTimestamp;
        std::vector<Struct> cache;
    };
}