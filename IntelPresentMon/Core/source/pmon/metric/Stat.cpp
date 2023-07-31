// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "Stat.h"
#include <PresentMonAPI/PresentMonAPI.h>

namespace p2c::pmon::met
{
	Stat::Stat(Type type) : type{ type } {}

    std::optional<double> Stat::Convert(const PM_METRIC_DOUBLE_DATA& data) const
    {
        if (data.valid) {
            switch (type)
            {
            case Type::avg: return data.avg;
            case Type::percentile_99: return data.percentile_99;
            case Type::percentile_95: return data.percentile_95;
            case Type::percentile_90: return data.percentile_90;
            case Type::high: return data.high;
            case Type::low: return data.low;
            case Type::raw: return data.raw;
            }
        }
        return std::nullopt;
    }

    std::wstring Stat::GetName() const
    {
        return NameFromType(type);
    }
    
    std::wstring Stat::NameFromType(Type type)
    {
        switch (type)
        {
        case Type::avg: return L"Avg";
        case Type::percentile_99: return L"99%";
        case Type::percentile_95: return L"95%";
        case Type::percentile_90: return L"90%";
        case Type::high: return L"High";
        case Type::low: return L"Low";
        case Type::raw: return L"Raw";
        default: return L"";
        }
    }

    
    std::vector<Stat::Option> Stat::GetOptions()
    {
        return {
            {Type::avg, NameFromType(Type::avg)},
            {Type::percentile_99, NameFromType(Type::percentile_99)},
            {Type::percentile_95, NameFromType(Type::percentile_95)},
            {Type::percentile_90, NameFromType(Type::percentile_90)},
            {Type::high, NameFromType(Type::high)},
            {Type::low, NameFromType(Type::low)},
            {Type::raw, NameFromType(Type::raw)},
        };
    }
}