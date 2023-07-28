// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "Metric.h"

namespace p2c::pmon
{
    Metric::Metric(std::wstring name, std::wstring units) : name{ std::move(name) }, units{ std::move(units) } {}
    Metric::~Metric() {}
    std::wstring Metric::GetStatType() const { return {}; }
    const std::wstring& Metric::GetName() const { return name; }
    const std::wstring& Metric::GetUnits() const { return units; }
    const std::wstring& Metric::GetMetricClassName() const
    {
        static std::wstring name = L"Text";
        return name;
    }
    Metric::Info Metric::GetInfo(size_t index) const
    {
        return {
            index,
            GetCategory(),
            GetName(),
            GetStatType(),
            GetUnits(),
            GetMetricClassName()
        };
    }
}