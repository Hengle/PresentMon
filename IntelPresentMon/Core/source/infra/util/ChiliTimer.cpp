// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "ChiliTimer.h"
#include <thread>
#include <Core/source/win/WinAPI.h>


namespace p2c::infra::util
{
	namespace qpc
	{
		uint64_t GetCurrentTimestamp()
		{
			LARGE_INTEGER timestamp;
			QueryPerformanceCounter(&timestamp);
			return (uint64_t)timestamp.QuadPart;
		}
		double GetPeriodSeconds()
		{
			LARGE_INTEGER freq;
			QueryPerformanceFrequency(&freq);
			return 1.0 / double(freq.QuadPart);
		}
		void WaitUntilTimestamp(uint64_t timestamp)
		{
			while (GetCurrentTimestamp() < timestamp)
			{
				std::this_thread::yield();
			}
		}
	}

	ChiliTimer::ChiliTimer() noexcept
	{
		performanceCounterPeriod = qpc::GetPeriodSeconds();
		Mark();
	}

	double ChiliTimer::Mark() noexcept
	{
		const auto newTimestamp = qpc::GetCurrentTimestamp();
		const auto delta = double(newTimestamp - startTimestamp) * performanceCounterPeriod;
		startTimestamp = newTimestamp;
		return delta;
	}

	double ChiliTimer::Peek() const noexcept
	{
		const auto newTimestamp = qpc::GetCurrentTimestamp();
		const auto delta = double(newTimestamp - startTimestamp) * performanceCounterPeriod;
		return delta;
	}

	uint64_t ChiliTimer::GetStartTimestamp() const noexcept
	{
		return startTimestamp;
	}
}
