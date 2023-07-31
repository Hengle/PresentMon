// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "NumericMetric.h"
#include <format>
#include <cmath>
#include <algorithm>

namespace p2c::pmon
{
	NumericMetric::NumericMetric(float scalingFactor, std::wstring name, std::wstring units)
		:
		Metric{ std::move(name), std::move(units) },
		scalingFactor{ scalingFactor }
	{}

	std::wstring NumericMetric::ReadStringValue(double timestamp)
	{
		const auto val = ReadValue(timestamp);
		if (val) {
			const auto digitsBeforeDecimal = int(log10(std::abs(*val)));
			const int maxFractionalDigits = 2;
			return std::format(L"{:.{}f}", *val, std::max(maxFractionalDigits - digitsBeforeDecimal, 0));
		}
		return L"NA";
	}

	const std::wstring& NumericMetric::GetMetricClassName() const
	{
		static std::wstring name = L"Numeric";
		return name;
	}

	float NumericMetric::GetScalingFactor() const
	{
		return scalingFactor;
	}
}