// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <string>
#include <PresentMonAPI/PresentMonAPI.h>

#define PM_PSU_TYPE_X_LIST \
	X_(PM_PSU_TYPE_NONE, None, L"None") \
	X_(PM_PSU_TYPE_PCIE, Pcie, L"PCIE") \
	X_(PM_PSU_TYPE_6PIN, Pin6, L"6 Pin") \
	X_(PM_PSU_TYPE_8PIN, Pin8, L"8 Pin")

namespace p2c::pmon
{
	enum class PsuType : uint32_t
	{
#define X_(a, b, c) b = uint32_t(a),
		PM_PSU_TYPE_X_LIST
#undef X_
	};

	PsuType ConvertPsuType(PM_PSU_TYPE mode);
	std::wstring PsuTypeToString(PsuType mode);
}

#ifndef KEEP_PM_PSU_TYPE_X_LIST
#undef PM_PSU_TYPE_X_LIST
#endif