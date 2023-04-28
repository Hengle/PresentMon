// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include "../Formatter.h"

namespace p2c::infra::log
{
	class EntryOutputBase;
}

namespace p2c::infra::log::fmt
{
	class DefaultFormatter : public Formatter
	{
	public:
		DefaultFormatter(bool showTrace = true, bool showNested = true);
		std::wstring Format(const EntryOutputBase&) const override;
	public:
		bool showTrace;
		bool showNested;
	};
}