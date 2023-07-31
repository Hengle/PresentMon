// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <Core/source/gfx/layout/GraphData.h>
#include <Core/source/meta/TypeFromMember.h>
#include "NumericMetric.h"
#include "../Timekeeper.h"
#include "../adapt/AdapterLookup.h"

namespace p2c::pmon::met
{
    template<auto pMetric>
    class SimpleMetric : public NumericMetric
    {
        using Struct = meta::MemberPointerInfo<decltype(pMetric)>::StructType;
        using Adapter = adapt::AdapterLookup<Struct>::Adapter;
    public:
        SimpleMetric(float scalingFactor, std::wstring name_, std::wstring units_, Adapter* pAdaptor)
            :
            NumericMetric{ scalingFactor, std::move(name_), std::move(units_) },
            pAdaptor{ pAdaptor }
        {}
        std::wstring GetStatType() const override { return L""; }
        void PopulateData(gfx::lay::GraphData& graphData, double timestamp) override
        {
            graphData.Push(gfx::lay::DataPoint{ .value = ReadValue(timestamp), .time = timestamp});
            graphData.Trim(timestamp);
        }
        std::optional<float> ReadValue(double timestamp) override
        {
            auto& polledData = pAdaptor->Poll(timestamp);
            return GetScalingFactor() * float(polledData.*pMetric);
        }
        const std::wstring& GetCategory() const override
        {
            return pAdaptor->category;
        }
    private:
        Adapter* pAdaptor;
    };
}