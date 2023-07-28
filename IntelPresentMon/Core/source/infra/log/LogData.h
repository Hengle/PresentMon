// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include <string>
#include <optional>
#include <chrono>
#include <Core/source/infra/util/StackTrace.h>
#include <Core/source/infra/util/ErrorCode.h>
#include "Level.h"
#include <cstdint>

namespace p2c::infra::log
{
	struct LogData
	{
		Level level = Level::Error;
		std::wstring sourceFile;
		std::wstring functionName;
		int sourceLine = -1;
		std::wstring note;
		std::chrono::system_clock::time_point timestamp;
		std::optional<util::ErrorCode> code;
		std::optional<util::StackTrace> stackTrace;
		std::optional<uint32_t> pid;
		std::optional<uint32_t> tid;
	};
}