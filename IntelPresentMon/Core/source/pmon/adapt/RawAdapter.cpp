// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "RawAdapter.h"
#include <format>
#include "../PresentMon.h"
#include <Core/source/infra/log/Logging.h>
#include <Core/source/infra/util/Assert.h>

namespace p2c::pmon::adapt
{    
    RawAdapter::RawAdapter(const PresentMon* pPmon)
        :
        pPmon{ pPmon },
        cache(initialCacheSize)
    {}

    std::span<const RawAdapter::Struct> RawAdapter::Pull(double timestamp)
    {
        if (!cacheTimestamp || cacheTimestamp != timestamp)
        {
            const auto pid = *pPmon->GetPid();
            // in: max writeable buffer size, out: actual num frames written
            auto frameCountInOut = (uint32_t)initialCacheSize;
            // cache buffer might have shrinked down real small, reset to initial size
            cache.resize(initialCacheSize);
            // where in vector to begin writing
            uint32_t writePosition = 0;
            while (true) {
                if (auto sta = pmGetFrameData(pid, &frameCountInOut, cache.data() + writePosition);
                    sta == PM_STATUS::PM_STATUS_NO_DATA) {
                    // "shrink to fit"
                    cache.resize(size_t(writePosition));
                    break;
                }
                else if (sta != PM_STATUS::PM_STATUS_SUCCESS) {
                    p2clog.warn(std::format(L"failed to get raw frame data with error [{}]", pid)).code(sta).commit();
                    cache.clear();
                    break;
                }
                // case where there *might* be more data
                // if the api writes to all available elements
                // means there might have been exactly the needed number
                // of elements in the buffer, or that there are more remaining
                if (size_t(writePosition) + size_t(frameCountInOut) >= std::size(cache)) {
                    // if we're full, we should have filled up to exact size of buffer
                    CORE_ASSERT(size_t(writePosition) + size_t(frameCountInOut) == std::size(cache));
                    // next frames write start just after end of current cache
                    writePosition = (uint32_t)std::size(cache);
                    // double size of cache to accomodate more entries
                    cache.resize(cache.size() * 2);
                    // available space is total size of cache minus size already written
                    frameCountInOut = (uint32_t)std::size(cache) - writePosition;
                }
                else {
                    // "shrink to fit"
                    cache.resize(size_t(writePosition) + size_t(frameCountInOut));
                    break;
                }
            }
            cacheTimestamp = timestamp;
        }
        return cache;
    }

    void RawAdapter::ClearCache()
    {
        cacheTimestamp.reset();
    }
}