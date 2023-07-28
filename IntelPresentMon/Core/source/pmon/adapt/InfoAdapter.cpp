// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "InfoAdapter.h"
#include <Core/source/infra/util/Assert.h>
#include "../PresentMon.h"

namespace p2c::pmon::adapt
{
	InfoAdapter::InfoAdapter(const PresentMon* pPmon)
		:
		pPmon{ pPmon }
	{}
	std::wstring InfoAdapter::GetElapsedTime() const
	{
		CORE_ASSERT((bool)startTime);
		const auto elapsed = std::chrono::system_clock::now() - *startTime;
		const auto timeMilliseconds = std::chrono::floor<std::chrono::milliseconds>(elapsed);
		return std::format(L"{:%T}", timeMilliseconds);
	}
	std::wstring InfoAdapter::GetCpuName() const
	{
		if (!cpuNameCache) {
			cpuNameCache = pPmon->GetCpuName();
		}
		return *cpuNameCache;
	}
	std::wstring InfoAdapter::GetGpuName() const
	{
		CORE_ASSERT((bool)adapters);
		if (adapters->empty()) {
			return {};
		}
		return infra::util::ToWide(
			adapters->at(pPmon->GetSelectedAdapter().value_or(0)).name
		);
	}
	std::wstring InfoAdapter::GetDateTime() const
	{
		const auto timeMilliseconds = std::chrono::floor<std::chrono::milliseconds>(
			std::chrono::system_clock::now()
		);
		return std::format(L"{}", std::chrono::zoned_time{
			std::chrono::current_zone(), timeMilliseconds
		});
	}
	void InfoAdapter::CaptureState()
	{
		try { adapters = pPmon->EnumerateAdapters();} catch (...) {}
		startTime = std::chrono::system_clock::now();
	}
}