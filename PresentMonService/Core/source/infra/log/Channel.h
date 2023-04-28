// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <memory>
#include <filesystem>
#include <vector>
#include <mutex>
#include "Driver.h"
#include "Policy.h"
#include "EntryOutputBase.h"

namespace p2c::infra::log
{
	class Channel
	{
	public:
		~Channel();
		void Accept(EntryOutputBase& entry);
		void AddDriver(std::unique_ptr<Driver> pDriver);
		void ClearDrivers();
		void AddPolicy(Policy policy);
		void ClearPolicies();
	private:
		std::mutex mutex;
		std::vector<Policy> policies;
		std::vector<std::unique_ptr<Driver>> driverPtrs;
	};
}