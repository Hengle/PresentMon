// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include "Metric.h"
#include <optional>


namespace p2c::gfx::lay
{
    class GraphData;
}

namespace p2c::pmon
{
    class NumericMetric : public Metric
    {
    public:
        // functions
        NumericMetric(float scalingFactor, std::wstring name, std::wstring units);
        virtual std::optional<float> ReadValue(double timestamp) = 0;
        virtual void PopulateData(gfx::lay::GraphData& data, double timestamp) = 0;
        std::wstring ReadStringValue(double timestamp) override;
        const std::wstring& GetMetricClassName() const override;
        float GetScalingFactor() const;
    private:
        float scalingFactor;
    };
}