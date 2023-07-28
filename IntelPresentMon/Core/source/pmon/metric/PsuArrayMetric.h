// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <memory>
#include <vector>
#include <format>
#include <Core/source/gfx/layout/GraphData.h>
#include <Core/source/meta/TypeFromMember.h>
#include "../Timekeeper.h"
#include "NumericMetric.h"
#include "Stat.h" 
#include "../adapt/GpuAdapter.h"
#include <PresentMonAPI/PresentMonAPI.h>

namespace p2c::pmon::met
{
    template<PM_METRIC_DOUBLE_DATA PM_PSU_DATA::* pMetric>
    class PsuArrayMetric : public NumericMetric
    {
        using Struct = PM_PSU_DATA;
        using Adapter = adapt::GpuAdapter;
    public:
        PsuArrayMetric(float scalingFactor, std::wstring name_, std::wstring units_, Stat::Type statType, size_t index, Adapter* pAdaptor)
            :
            NumericMetric{ scalingFactor, std::move(name_), std::move(units_) },
            stat{ statType },
            index{ index },
            pAdaptor{ pAdaptor }
        {}
        static std::vector<std::unique_ptr<Metric>> MakeAll(float scalingFactor, std::wstring name_, std::wstring units_, size_t index, Adapter* pAdaptor)
        {
            std::vector<std::unique_ptr<Metric>> metrics;
            for (auto& o : Stat::GetOptions())
            {
                metrics.push_back(std::make_unique<PsuArrayMetric>(scalingFactor, std::format(L"{} {}", name_, index), units_, o.value, index, pAdaptor));
            }
            return metrics;
        }
        static std::vector<std::unique_ptr<Metric>> MakeAllArray(float scalingFactor, std::wstring name_, std::wstring units_, size_t size, Adapter* pAdaptor)
        {
            std::vector<std::unique_ptr<Metric>> metrics;
            for (size_t i = 0; i < size; i++)
            {
                auto metricsForIndex = MakeAll(scalingFactor, name_, units_, i, pAdaptor);
                metrics.insert(metrics.end(), std::make_move_iterator(metricsForIndex.begin()), std::make_move_iterator(metricsForIndex.end()));
            }
            return metrics;
        }
        std::wstring GetStatType() const override { return stat.GetName(); }
        void PopulateData(gfx::lay::GraphData& graphData, double timestamp) override
        {
            graphData.Push(gfx::lay::DataPoint{ .value = ReadValue(timestamp), .time = timestamp });
            graphData.Trim(timestamp);
        }
        std::optional<float> ReadValue(double timestamp) override
        {
            auto& polledData = pAdaptor->Poll(timestamp);
            return stat.Convert(polledData.psu_data[index].*pMetric)
                .transform([this](double v) { return GetScalingFactor() * float(v); });
        }
        const std::wstring& GetCategory() const override
        {
            return pAdaptor->category;
        }
    private:
        Stat stat;
        size_t index;
        Adapter* pAdaptor;
    };
}