// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include "Metric.h"
#include "../Timekeeper.h"

struct PM_FPS_DATA;

namespace p2c::pmon::adapt
{
    class FpsAdapter;
}

namespace p2c::pmon::met
{
    class PresentModeMetric : public Metric
    {
        using Struct = PM_FPS_DATA;
        using Adapter = adapt::FpsAdapter;
    public:
        PresentModeMetric(Adapter* pAdaptor);
        std::wstring GetStatType() const override;
        const std::wstring& GetCategory() const override;
        std::wstring ReadStringValue(double timestamp) override;
    private:
        Adapter* pAdaptor;
    };
}