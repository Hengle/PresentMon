// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <Core/source/gfx/layout/GraphData.h>
#include <Core/source/meta/TypeFromMember.h>
#include "NumericMetric.h"
#include "../adapt/RawAdapter.h"
#include "../Timekeeper.h"

// DEPRECATED: previously raw metrics were exposed directly in UI, but now any raw metrics that need exposure will
// instead be exposed by adding metrics in the polling api

namespace p2c::pmon::met
{
    template<auto pMetric>
    class RawMetric : public NumericMetric
    {
        using Adaptor = adapt::RawAdapter;
        using Struct = Adaptor::Struct;
        using Value = meta::MemberPointerInfo<decltype(pMetric)>::MemberType;
        static_assert(std::same_as<Struct, meta::MemberPointerInfo<decltype(pMetric)>::StructType>);
    public:
        RawMetric(float scalingFactor, std::wstring name_, std::wstring units_, Adaptor* pAdaptor)
            :
            NumericMetric{ scalingFactor, std::move(name_), std::move(units_) },
            pAdaptor{ pAdaptor }
        {}
        std::wstring GetStatType() const override { return L""; }
        void PopulateData(gfx::lay::GraphData& graphData, double timestamp) override
        {
            for (auto& f : pAdaptor->Pull(timestamp))
            {
                const auto value = GetScalingFactor() * float(f.*pMetric);
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
                mostRecent = GetScalingFactor() * float(values.back().*pMetric);
            }
            return mostRecent;
        }
    private:
        float mostRecent = 0.f;
        Adaptor* pAdaptor;
    };
}