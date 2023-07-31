// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <functional>
#include <vector>
#include "EntryOutputBase.h"

namespace p2c::infra::log
{
	class Policy
	{
	public:
		Policy(std::function<bool(EntryOutputBase& entry)> transform)
			:
			transform{ std::move(transform) }
		{}
		// returns false if entry is discarded (filtered out)
		bool Process(EntryOutputBase& entry) const
		{
			return transform(entry);
		}
	private:
		std::function<bool(EntryOutputBase& entry)> transform;
	};
}