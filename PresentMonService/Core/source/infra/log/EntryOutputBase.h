// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <functional>
#include "LogData.h"

namespace p2c::infra::log
{
	// core interface and data for a logging channel to read the log data
	// we use this inheritance to keep the reading interface out of the
	// forward-facing client setup interface
	class EntryOutputBase
	{
	public:
		// functions
		EntryOutputBase() = default;
		EntryOutputBase(std::wstring sourceFile, int sourceLine, std::wstring functionName);
		// data
		LogData data;
		bool flushing = false;
		bool tracing = false;
		bool throwing = false;
		bool logging = true;
		const std::exception* pNested = nullptr;
		std::function<void(LogData, const std::exception*)> exceptinator;
		std::wstring snapshot;
	};
}