// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <string>

namespace p2c::infra::log
{
	class EntryOutputBase;

	class Formatter
	{
	public:
		virtual ~Formatter() = default;
		virtual std::wstring Format(const EntryOutputBase&) const = 0;
	};
}