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

// NOTE: was designed to support both csv captures and realtime overlay readout
// readout support is deprecated, and might be removed in a future refactoring

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