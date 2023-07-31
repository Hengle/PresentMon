// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "EntryOutputBase.h"
#include <chrono>
#include <Core/source/infra/util/Util.h>

namespace p2c::infra::log
{
	EntryOutputBase::EntryOutputBase(std::wstring sourceFile, int sourceLine, std::wstring functionName)
	{
		data.sourceFile = std::move(sourceFile);
		data.sourceLine = sourceLine;
		data.functionName = std::move(functionName);
		data.timestamp = std::chrono::system_clock::now();
	}
}