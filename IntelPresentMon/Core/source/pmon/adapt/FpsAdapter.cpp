// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "FpsAdapter.h"
#include "../PresentMon.h"
#include <Core/source/infra/log/Logging.h>

namespace p2c::pmon::adapt
{
    FpsAdapter::FpsAdapter(const PresentMon* pPmon)
        :
        pPmon{ pPmon }
    {}

    const FpsAdapter::Struct& FpsAdapter::Poll(double timestamp)
    {
        const auto pid = pPmon->GetPid();
        if (!pid)
        {
            p2clog.note(L"Polling adapter while pid not open").commit();
        }
        if (!cacheTimestamp || *cacheTimestamp != timestamp)
        {
            uint32_t swapChainCount = 1;
            if (auto sta = pmGetFramesPerSecondData(*pid, &cache, pPmon->GetWindow(), &swapChainCount); sta != PM_STATUS::PM_STATUS_SUCCESS)
            {
                p2clog.warn(L"Failed to get fps data").code(sta).commit();
                cache = {};
            }
            if (swapChainCount != 1 && !swapChainWarningFired)
            {
                p2clog.warn(L"Wrong number of swap chains in pmon polling call.").commit();
                swapChainWarningFired = true;
            }
            cacheTimestamp = timestamp;
        }
        return cache;
    }

    void FpsAdapter::ClearCache()
    {
        cacheTimestamp.reset();
    }
}