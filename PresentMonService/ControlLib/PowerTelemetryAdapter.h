// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>
#include <optional>
#include "PresentMonPowerTelemetry.h"
#include "../PresentMonAPI/PresentMonAPI.h"

namespace pwr
{
    class PowerTelemetryAdapter
    {
    public:
        virtual ~PowerTelemetryAdapter() = default;
        virtual bool Sample() noexcept = 0;
        virtual std::optional<PresentMonPowerTelemetryInfo> GetClosest(uint64_t qpc) const noexcept = 0;
        virtual PM_GPU_VENDOR GetVendor() const noexcept = 0;
        virtual std::string GetName() const noexcept = 0;

        // constants
        static constexpr size_t defaultHistorySize = 300;
    };
}