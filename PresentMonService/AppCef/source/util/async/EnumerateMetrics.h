// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include "../AsyncEndpoint.h"
#include <Core/source/kernel/Kernel.h>
#include "../CefValues.h"

namespace p2c::client::util::async
{
    class EnumerateMetrics : public AsyncEndpoint
    {
    public:
        static constexpr std::string GetKey() { return "enumerateMetrics"; }
        EnumerateMetrics() : AsyncEndpoint{ AsyncEndpoint::Environment::KernelTask } {}
        // {} => {metrics: [{category: string, name: string, statType: string, units: string, index: uint, className: string}]}
        Result ExecuteOnKernelTask(uint64_t uid, CefRefPtr<CefValue> pArgObj, kern::Kernel& kernel) const override
        {
            auto metricList = kernel.EnumerateMetrics();
            auto metricListCef = MakeCefList(metricList.size());
            for (int i = 0; i < metricList.size(); i++)
            {
                auto& metric = metricList[i];
                metricListCef->SetValue(i, MakeCefObject(
                    CefProp{ "category", std::move(metric.category) },
                    CefProp{ "name", std::move(metric.name) },
                    CefProp{ "statType", std::move(metric.statType) },
                    CefProp{ "units", std::move(metric.units) },
                    CefProp{ "index", (int)metric.index },
                    CefProp{ "className", std::move(metric.className) }
                ));
            }
            return Result{ true, MakeCefObject(CefProp{ "metrics", CefValueDecay(std::move(metricListCef)) }) };
        }
    };
}