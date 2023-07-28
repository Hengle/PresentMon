// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <Core/source/infra/util/ChiliTimer.h>

namespace p2c::pmon
{
	class Timekeeper
	{
	public:
		static double Now();
		static void LockNow();
		static double GetLockedNow();
		static double RelativeToEpoch(uint64_t qpc);
	private:
		Timekeeper();
		static Timekeeper singleton;
		uint64_t epoch;
		double period;
		double locked = 0.;
	};
}
