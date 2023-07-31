// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <vector>
#include <optional>

struct PM_METRIC_DOUBLE_DATA;

namespace p2c::pmon::met
{
    class Stat
    {
    public:
        // types
        enum class Type
        {
            avg,
            percentile_99,
            percentile_95,
            percentile_90,
            high,
            low,
            raw,
        };
        struct Option
        {
            Type value;
            std::wstring string;
        };
        // functions
        Stat(Type type);
        std::optional<double> Convert(const PM_METRIC_DOUBLE_DATA& data) const;
        std::wstring GetName() const;
        static std::wstring NameFromType(Type type);
        static std::vector<Option> GetOptions();
    private:
        Type type;
    };
}