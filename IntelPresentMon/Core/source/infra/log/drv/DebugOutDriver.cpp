// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "DebugOutDriver.h"
#include "../fmt/DefaultFormatter.h"
#include <Core/source/win/WinAPI.h>

namespace p2c::infra::log::drv
{
	DebugOutDriver::DebugOutDriver()
		:
		Driver{ std::make_unique<fmt::DefaultFormatter>(false) }
	{}

	void DebugOutDriver::Commit(const EntryOutputBase& entry)
	{
		OutputDebugStringW(FormatEntry(entry).c_str());
	}
}