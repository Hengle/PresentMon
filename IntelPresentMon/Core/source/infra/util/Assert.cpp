// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "Assert.h"
#include "../log/Logging.h"
#include <string>
#include "MacroHelpers.h"

namespace p2c::infra::util
{
	void Assert(bool passed, const wchar_t* msg, const wchar_t* file, int line, const wchar_t* function)
	{
		if (!passed)
		{
			using namespace std::string_literals;
			p2c::infra::log::Entry{ file, line, function }
				.note(L"Assert failed: "s + msg)
				.ex(AssertionException{})
				.pid().flush().commit();
		}
	}
}