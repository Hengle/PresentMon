// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "PowerTelemetryProviderFactory.h"
#include "IntelPowerTelemetryProvider.h"
#include "NvidiaPowerTelemetryProvider.h"
#include "AmdPowerTelemetryProvider.h"

namespace pwr
{
	std::unique_ptr<PowerTelemetryProvider> PowerTelemetryProviderFactory::Make(PM_GPU_VENDOR vendor)
	{
		switch (vendor) {
		case PM_GPU_VENDOR_INTEL: return std::make_unique<intel::IntelPowerTelemetryProvider>();
		case PM_GPU_VENDOR_NVIDIA: return std::make_unique<nv::NvidiaPowerTelemetryProvider>();
        case PM_GPU_VENDOR_AMD: return std::make_unique <amd::AmdPowerTelemetryProvider>();
		}
		return {};
	}
}