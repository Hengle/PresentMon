// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include "PowerTelemetryProvider.h"
#include "../PresentMonAPI/PresentMonAPI.h"
#include <memory>

namespace pwr
{
	class PowerTelemetryProviderFactory
	{
	public:
		static std::unique_ptr<PowerTelemetryProvider> Make(PM_GPU_VENDOR vendor);
	};
}