// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include "../Driver.h"

namespace p2c::infra::log
{
	class EntryOutputBase;
}

namespace p2c::infra::log::drv
{
	class DebugOutDriver : public Driver
	{
	public:
		DebugOutDriver();
		void Commit(const EntryOutputBase&) override;
	};
}
