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
#include "../PsuType.h"

namespace p2c::pmon::met
{
    class PsuTypeArrayMetric : public Metric
    {
        using Adapter = adapt::GpuAdapter;
    public:
        PsuTypeArrayMetric(size_t index, Adapter* pAdaptor)
            :
            Metric{ std::format(L"PSU Type {}", index), L"" },
            index{ index },
            pAdaptor{ pAdaptor }
        {}
        static std::vector<std::unique_ptr<Metric>> MakeArray(size_t size, Adapter* pAdaptor)
        {
            std::vector<std::unique_ptr<Metric>> metrics;
            for (size_t i = 0; i < size; i++)
            {
                metrics.push_back(std::make_unique<PsuTypeArrayMetric>(i, pAdaptor));
            }
            return metrics;
        }
        std::wstring ReadStringValue(double timestamp) override
        {
            return PsuTypeToString(ConvertPsuType(pAdaptor->Poll(timestamp).psu_data[index].psu_type));
        }
        const std::wstring& GetCategory() const override
        {
            return pAdaptor->category;
        }
    private:
        size_t index;
        Adapter* pAdaptor;
    };
}