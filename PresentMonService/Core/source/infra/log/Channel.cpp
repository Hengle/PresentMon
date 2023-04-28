// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "Channel.h"
#include <Core/source/infra/util/Exception.h>

namespace p2c::infra::log
{
	Channel::~Channel()
	{}

	void Channel::Accept(EntryOutputBase& entry)
	{
		std::lock_guard lock{ mutex };
		for (const auto& pol : policies)
		{
			if (!pol.Process(entry))
			{
				return;
			}
		}

		if (entry.tracing)
		{
			entry.data.stackTrace.emplace();
		}

		for (size_t i = 0; i < driverPtrs.size(); i++)
		{
			if (i == driverPtrs.size() - 1 && !entry.throwing)
			{
				// this move is okay because above check makes sure we are on last driver
				// and makes sure that exception will not be thrown below
#pragma warning(push)
#pragma warning(disable : 26800)
				driverPtrs[i]->Accept(std::move(entry));
#pragma warning(pop)
			}
			else
			{
				driverPtrs[i]->Accept(entry);
			}
		}

		if (entry.throwing)
		{
			if (entry.exceptinator)
			{
				entry.exceptinator(std::move(entry.data), entry.pNested);
			}
			else
			{
				util::Exception e;
				e.logData = std::move(entry.data);
				if (entry.pNested)
				{
					e.SetInner(*entry.pNested);
				}
				throw e;
			}
		}
	}

	void Channel::AddDriver(std::unique_ptr<Driver> pDriver)
	{
		std::lock_guard lock{ mutex };
		driverPtrs.push_back(std::move(pDriver));
	}

	void Channel::AddPolicy(Policy policy)
	{
		std::lock_guard lock{ mutex };
		policies.push_back(std::move(policy));
	}

	void Channel::ClearDrivers()
	{
		std::lock_guard lock{ mutex };
		driverPtrs.clear();
	}

	void Channel::ClearPolicies()
	{
		std::lock_guard lock{ mutex };
		policies.clear();
	}
}