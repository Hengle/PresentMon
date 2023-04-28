// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <string>

namespace p2c::infra::log
{
	enum class Level
	{
		Error,
		Warning,
		Info,
		Debug,
		Verbose,
	};

	std::wstring GetLevelName(Level lv);
}