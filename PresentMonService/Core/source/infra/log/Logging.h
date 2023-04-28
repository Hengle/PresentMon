// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include "Entry.h"
#include "../util/MacroHelpers.h"

#define p2clog p2c::infra::log::Entry{ CORE_WIDEN(__FILE__), __LINE__, CORE_WIDEN(__FUNCTION__) }

#define p2cvlog(mod) !mod ? (void)0 : p2clog.verbose()

namespace p2c::infra::log
{
	struct HrSink
	{
		inline void operator<<(HRESULT hr) const
		{
			if (hr < 0)
			{
				p2clog.hr(hr).commit();
			}
		}
	};
}

#define p2chrlog p2c::infra::log::HrSink{}
