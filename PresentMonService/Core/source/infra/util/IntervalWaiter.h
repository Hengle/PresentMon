// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include "ChiliTimer.h"

namespace p2c::infra::util
{
	class IntervalWaiter
	{
	public:
		IntervalWaiter(double interval);
		IntervalWaiter(const IntervalWaiter&) = delete;
		IntervalWaiter& operator=(const IntervalWaiter&) = delete;
		void SetInterval(double interval);
		void Wait();
		~IntervalWaiter();
	private:
		static constexpr double waitBuffer_ = 0.000'25;
		ChiliTimer timer_;
		double interval_;
		double lastTargetTime_ = 0.;
		void* waitableTimerHandle_ = nullptr;
	};
}