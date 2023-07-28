// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <Core/source/pmon/metric/Metric.h>
#include <format>

namespace p2c::kern
{
	struct TextDataPack
	{
		void Populate(double timestamp)
		{
			text = pMetric->ReadStringValue(timestamp);
		}
		std::wstring GetFullName() const
		{
			auto stat = pMetric->GetStatType();
			auto statPart = stat.empty() ? std::wstring{} : std::format(L" ({})", stat);
			return std::format(L"{}{}", pMetric->GetName(), statPart);
		}
		std::wstring text;
		pmon::Metric* pMetric = nullptr;
	};
}