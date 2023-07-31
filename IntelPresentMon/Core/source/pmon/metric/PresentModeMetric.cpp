// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "PresentModeMetric.h"
#include "../adapt/FpsAdapter.h"
#include "../PresentMode.h"


namespace p2c::pmon::met
{
    PresentModeMetric::PresentModeMetric(Adapter* pAdaptor)
        :
        Metric{ L"Present Mode", L"" },
        pAdaptor{ pAdaptor }
    {}

    std::wstring PresentModeMetric::GetStatType() const
    {
        return L"";
    }

    const std::wstring& PresentModeMetric::GetCategory() const
    {
        return pAdaptor->category;
    }

    std::wstring PresentModeMetric::ReadStringValue(double timestamp)
    {
        return PresentModeToString(ConvertPresentMode(pAdaptor->Poll(timestamp).present_mode));
    }
}