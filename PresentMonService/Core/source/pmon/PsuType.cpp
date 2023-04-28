// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#define KEEP_PM_PSU_TYPE_X_LIST
#include "PsuType.h"

namespace p2c::pmon
{
	PsuType ConvertPsuType(PM_PSU_TYPE mode)
	{
		return PsuType(mode);
	}

	std::wstring PsuTypeToString(PsuType mode)
	{
		switch (mode)
		{
#define X_(a, b, c) case PsuType::b: return c;
			PM_PSU_TYPE_X_LIST
#undef X_
		default: return L"Invalid";
		}
	}
}