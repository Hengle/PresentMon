// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "Timekeeper.h"

namespace p2c::pmon
{
	double Timekeeper::Now()
	{
		return RelativeToEpoch(infra::util::qpc::GetCurrentTimestamp());
	}

	void Timekeeper::LockNow()
	{
		singleton.locked = Now();
	}

	double Timekeeper::GetLockedNow()
	{
		return singleton.locked;
	}

	double Timekeeper::RelativeToEpoch(uint64_t qpc)
	{
		return singleton.period * double(qpc - singleton.epoch);
	}

	Timekeeper::Timekeeper()
		:
		epoch{ infra::util::qpc::GetCurrentTimestamp() },
		period{ infra::util::qpc::GetPeriodSeconds() }
	{}

	Timekeeper Timekeeper::singleton{};
}
