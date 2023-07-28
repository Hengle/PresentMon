// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include "Metric.h"

namespace p2c::pmon::adapt
{
    class InfoAdapter;
}

namespace p2c::pmon::met
{
    template<auto pMetricGetter>
    class InfoMetric : public Metric
    {
    public:
        InfoMetric(std::wstring name, adapt::InfoAdapter* pAdapter)
            :
            Metric{ std::move(name), {} },
            pAdapter{ pAdapter }
        {}
        std::wstring GetStatType() const override
        {
            return L"";
        }
        const std::wstring& GetCategory() const override
        {
            return category;
        }
        std::wstring ReadStringValue(double timestamp) override
        {
            return (pAdapter->*pMetricGetter)();
        }
    private:
        inline static std::wstring category = L"Info";
        adapt::InfoAdapter* pAdapter;
    };
}