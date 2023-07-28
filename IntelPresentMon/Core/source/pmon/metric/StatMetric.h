// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <memory>
#include <vector>
#include <Core/source/gfx/layout/GraphData.h>
#include <Core/source/meta/TypeFromMember.h>
#include "../Timekeeper.h"
#include "NumericMetric.h"
#include "Stat.h"
#include "../adapt/AdapterLookup.h"

namespace p2c::pmon::met
{
    template<auto pMetric>
    class StatMetric : public NumericMetric
    {
        using Struct = meta::MemberPointerInfo<decltype(pMetric)>::StructType;
        using Adapter = adapt::AdapterLookup<Struct>::Adapter;
    public:
        StatMetric(float scalingFactor, std::wstring name_, std::wstring units_, Stat::Type statType, Adapter* pAdaptor)
            :
            NumericMetric{ scalingFactor, std::move(name_), std::move(units_) },
            stat{ statType },
            pAdaptor{ pAdaptor }
        {}
        static std::vector<std::unique_ptr<Metric>> MakeAll(float scalingFactor, std::wstring name_, std::wstring units_, Adapter* pAdaptor)
        {
            std::vector<std::unique_ptr<Metric>> metrics;
            for (auto& o : Stat::GetOptions())
            {
                metrics.push_back(std::make_unique<StatMetric>(scalingFactor, name_, units_, o.value, pAdaptor));
            }
            return metrics;
        }
        std::wstring GetStatType() const override { return stat.GetName(); }
        void PopulateData(gfx::lay::GraphData& graphData, double timestamp) override
        {
            graphData.Push(gfx::lay::DataPoint{ .value = ReadValue(timestamp), .time = timestamp});
            graphData.Trim(timestamp);
        }
        std::optional<float> ReadValue(double timestamp) override
        {
            auto& polledData = pAdaptor->Poll(timestamp);
            return stat.Convert(polledData.*pMetric)
                .transform([this](double v) { return GetScalingFactor() * float(v); });
        }
        const std::wstring& GetCategory() const override
        {
            return pAdaptor->category;
        }
    private:
        Stat stat;
        Adapter* pAdaptor;
    };
}