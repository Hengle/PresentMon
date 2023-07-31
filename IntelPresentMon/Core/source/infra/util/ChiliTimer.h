// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>

namespace p2c::infra::util
{
	namespace qpc
	{
		uint64_t GetCurrentTimestamp();
		double GetPeriodSeconds();
		void WaitUntilTimestamp(uint64_t timestamp);
	}

	class ChiliTimer
	{
	public:
		ChiliTimer() noexcept;
		double Mark() noexcept;
		double Peek() const noexcept;
		uint64_t GetStartTimestamp() const noexcept;
	private:
		double performanceCounterPeriod;
		uint64_t startTimestamp = 0;
	};
}