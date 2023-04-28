// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "Level.h"

namespace p2c::infra::log
{
	std::wstring GetLevelName(Level lv)
	{
		switch (lv)
		{
		case Level::Error: return L"Error";
		case Level::Warning: return L"Warning";
		case Level::Info: return L"Info";
		case Level::Debug: return L"Debug";
		case Level::Verbose: return L"Verbose";
		default: return L"Unknown";
		}
	}
}