// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "GpuAdapter.h"
#include "../PresentMon.h"
#include <Core/source/infra/log/Logging.h>

namespace p2c::pmon::adapt
{
    GpuAdapter::GpuAdapter(const PresentMon* pPmon)
        :
        pPmon{ pPmon }
    {}

    const GpuAdapter::Struct& GpuAdapter::Poll(double timestamp)
    {
        const auto pid = pPmon->GetPid();
        if (!pid)
        {
            p2clog.note(L"Polling adapter while pid not open").commit();
        }
        if (!cacheTimestamp || *cacheTimestamp != timestamp)
        {
            if (auto sta = pmGetGPUData(*pid, &cache, pPmon->GetWindow()); sta != PM_STATUS::PM_STATUS_SUCCESS)
            {
                p2clog.warn(L"Failed to get gpu data").code(sta).commit();
                cache = {};
            }
            cacheTimestamp = timestamp;
        }
        return cache;
    }

    void GpuAdapter::ClearCache()
    {
        cacheTimestamp.reset();
    }
}