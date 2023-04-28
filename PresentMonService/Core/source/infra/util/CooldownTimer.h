// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include "ChiliTimer.h"
#include <deque>


namespace p2c::infra::util
{
	class CooldownTimer
	{
	public:
		CooldownTimer(float period, size_t threshold);
		bool Trigger();
		void Update();
		bool ThresholdExceeded() const;
	private:
		ChiliTimer timer_;
		std::deque<float> instances_;
		float period_;
		size_t threshold_;
	};
}