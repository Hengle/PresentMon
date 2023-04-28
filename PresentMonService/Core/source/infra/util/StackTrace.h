// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <memory>

namespace backward
{
	class StackTrace;
}

namespace p2c::infra::util
{
	class StackTrace
	{
	public:
		StackTrace();
		StackTrace(const StackTrace& src);
		StackTrace(StackTrace&& donor);
		StackTrace& operator=(const StackTrace&);
		StackTrace& operator=(StackTrace&& donor);
		~StackTrace();
		std::wstring Print() const;
		const backward::StackTrace& GetNative() const;
	private:
		std::unique_ptr<backward::StackTrace> pTrace;
	};
}
