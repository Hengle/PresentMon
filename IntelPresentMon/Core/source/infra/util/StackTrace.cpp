// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "StackTrace.h"
#include <Core/source/infra/util/Util.h>
#include <Core/source/win/WinAPI.h>

#pragma warning(push)
#pragma warning(disable : 26495 26439 26451)
#include <Core/Third/backward/backward.hpp>
#pragma warning(pop)

namespace p2c::infra::util
{
	StackTrace::StackTrace()
	{
		backward::TraceResolver thisIsAWorkaround; // https://github.com/bombela/backward-cpp/issues/206
		pTrace = std::make_unique<backward::StackTrace>();
		pTrace->load_here(64);
	}

	StackTrace::StackTrace(const StackTrace& src)
		:
		pTrace{ new backward::StackTrace(*src.pTrace) }
	{}

	StackTrace::StackTrace(StackTrace&& donor) = default;

	StackTrace& StackTrace::operator=(const StackTrace& src)
	{
		pTrace = std::make_unique<backward::StackTrace>(*src.pTrace);
		return *this;
	}

	StackTrace& StackTrace::operator=(StackTrace&& donor) = default;

	StackTrace::~StackTrace() = default;

	std::wstring StackTrace::Print() const
	{
		if (!pTrace)
		{
			return {};
		}
		std::ostringstream oss;
		backward::Printer printer;
		printer.print(*pTrace, oss);
		return util::ToWide(oss.str());
	}

	const backward::StackTrace& StackTrace::GetNative() const
	{
		return *pTrace;
	}
}