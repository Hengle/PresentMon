// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <Core/source/gfx/layout/GraphData.h>
#include <Core/source/meta/TypeFromMember.h>
#include "NumericMetric.h"
#include "../adapt/RawAdapter.h"
#include "../Timekeeper.h"
#include <format>

namespace p2c::pmon::met
{
    template<auto pMetric>
    class RawArrayMetric : public NumericMetric
    {
        using Adaptor = adapt::RawAdapter;
        using Struct = Adaptor::Struct;
        using Value = meta::MemberPointerInfo<decltype(pMetric)>::MemberType;
        static_assert(std::same_as<Struct, meta::MemberPointerInfo<decltype(pMetric)>::StructType>);
        static_assert(std::is_array_v<Value>);
    public:
        RawArrayMetric(float scalingFactor, std::wstring name_, std::wstring units_, size_t index, Adaptor* pAdaptor)
            :
            NumericMetric{ scalingFactor, std::move(name_), std::move(units_) },
            index{ index },
            pAdaptor{ pAdaptor }
        {}
        static std::vector<std::unique_ptr<Metric>> MakeArray(float scalingFactor, std::wstring name_, std::wstring units_, size_t size, Adaptor* pAdaptor)
        {
            std::vector<std::unique_ptr<Metric>> metrics;
            for (size_t i = 0; i < size; i++)
            {
                metrics.push_back(std::make_unique<RawArrayMetric>(scalingFactor, std::format(L"{} {}", name_, i), units_, i, pAdaptor));
            }
            return metrics;
        }
        std::wstring GetStatType() const override { return L""; }
        void PopulateData(gfx::lay::GraphData& graphData, double timestamp) override
        {
            for (auto& f : pAdaptor->Pull(timestamp))
            {
                const auto value = GetScalingFactor() * float((f.*pMetric)[index]);
                graphData.Push(gfx::lay::DataPoint{ .value = value, .time = Timekeeper::RelativeToEpoch(f.qpc_time) });
            }
            graphData.Trim(timestamp);
        }
        const std::wstring& GetCategory() const override
        {
            static std::wstring cat = L"Raw";
            return cat;
        }
        float ReadValue(double timestamp) override
        {
            if (auto values = pAdaptor->Pull(timestamp); !values.empty())
            {
                mostRecent = GetScalingFactor() * float((values.back().*pMetric)[index]);
            }
            return mostRecent;
        }
    private:
        size_t index;
        float mostRecent = 0.f;
        Adaptor* pAdaptor;
    };
}