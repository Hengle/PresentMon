// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once

#ifdef CORE_ASSERT_IS_CASSERT
#include <cassert>
#define CORE_ASSERT(expr) assert(expr)
#else
#include "Exception.h"

namespace p2c::infra::util
{
	void Assert(bool tripped, const wchar_t* msg, const wchar_t* file, int line, const wchar_t* function);

	class AssertionException : public Exception {
	public: using Exception::Exception;
	};
}

#ifndef NDEBUG
#define CORE_ASSERT(expr) p2c::infra::util::Assert((expr), CORE_WIDEN(#expr), CORE_WIDEN(__FILE__), __LINE__, CORE_WIDEN(__FUNCTION__));
#else
#define CORE_ASSERT(expr) ((void)0)
#endif

#endif