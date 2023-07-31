// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "CpuAdapter.h"
#include "../PresentMon.h"
#include <Core/source/infra/log/Logging.h>

namespace p2c::pmon::adapt
{
    CpuAdapter::CpuAdapter(const PresentMon* pPmon)
        :
        pPmon{ pPmon }
    {}

    const CpuAdapter::Struct& CpuAdapter::Poll(double timestamp)
    {
        const auto pid = pPmon->GetPid();
        if (!pid)
        {
            p2clog.note(L"Polling adapter while pid not open").commit();
        }
        if (!cacheTimestamp || *cacheTimestamp != timestamp)
        {
            if (auto sta = pmGetCPUData(*pid, &cache, pPmon->GetWindow()); sta != PM_STATUS::PM_STATUS_SUCCESS)
            {
                p2clog.warn(L"Failed to get fps data").code(sta).commit();
                cache = {};
            }
            cacheTimestamp = timestamp;
        }
        return cache;
    }

    void CpuAdapter::ClearCache()
    {
        cacheTimestamp.reset();
    }
}