// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#define NOMINMAX
#include <Windows.h>
#include "igcl_api.h"
#include "PowerTelemetryAdapter.h"
#include "TelemetryHistory.h"
#include <mutex>
#include <optional>

namespace pwr::intel
{
	class IntelPowerTelemetryAdapter : public PowerTelemetryAdapter
	{
	public:
		IntelPowerTelemetryAdapter(ctl_device_adapter_handle_t handle);
		bool Sample() noexcept override;
		std::optional<PresentMonPowerTelemetryInfo> GetClosest(uint64_t qpc) const noexcept override;
		PM_GPU_VENDOR GetVendor() const noexcept override;
		std::string GetName() const noexcept override;
        uint64_t GetDedicatedVideoMemory() const noexcept override;

		// types
		class NonGraphicsDeviceException : public std::exception {};

	private:
		// functions
		ctl_result_t EnumerateMemoryModules();

		ctl_result_t GetTimeDelta(const ctl_power_telemetry_t& power_telemetry);

		// TODO: meld these into the sample function
		ctl_result_t GetGPUPowerTelemetryData(
			const ctl_power_telemetry_t& power_telemetry,
			PresentMonPowerTelemetryInfo& pm_gpu_power_telemetry_info);
		ctl_result_t GetVramPowerTelemetryData(
			const ctl_power_telemetry_t& power_telemetry,
			PresentMonPowerTelemetryInfo& pm_gpu_power_telemetry_info);
		ctl_result_t GetFanPowerTelemetryData(
			const ctl_power_telemetry_t& power_telemetry,
			PresentMonPowerTelemetryInfo& pm_gpu_power_telemetry_info);
		ctl_result_t GetPsuPowerTelemetryData(
			const ctl_power_telemetry_t& power_telemetry,
			PresentMonPowerTelemetryInfo& pm_gpu_power_telemetry_info);

		void GetMemStateTelemetryData(
			const ctl_mem_state_t& mem_state,
			PresentMonPowerTelemetryInfo& pm_gpu_power_telemetry_info);
		void GetMemBandwidthData(
			const ctl_mem_bandwidth_t& mem_bandwidth,
			PresentMonPowerTelemetryInfo& pm_gpu_power_telemetry_info);

		void SavePmPowerTelemetryData(PresentMonPowerTelemetryInfo& pm_gpu_power_telemetry_info);
        ctl_result_t SaveTelemetry(
            const ctl_power_telemetry_t& power_telemetry,
            const ctl_mem_bandwidth_t& mem_bandwidth);

		// TODO: put these as part of the telemetry data object
		ctl_result_t GetInstantaneousPowerTelemetryItem(
			const ctl_oc_telemetry_item_t& telemetry_item,
			double& pm_telemetry_value,
			GpuTelemetryCapBits telemetry_cap_bit);
		ctl_result_t GetPowerTelemetryItemUsagePercent(
			const ctl_oc_telemetry_item_t& current_telemetry_item,
			const ctl_oc_telemetry_item_t& previous_telemetry_item,
			double& pm_telemetry_value,
			GpuTelemetryCapBits telemetry_cap_bit);
		ctl_result_t GetPowerTelemetryItemUsage(
			const ctl_oc_telemetry_item_t& current_telemetry_item,
			const ctl_oc_telemetry_item_t& previous_telemetry_item,
			double& pm_telemetry_value,
			GpuTelemetryCapBits telemetry_cap_bit);
        GpuTelemetryCapBits GetFlagTelemetryCapBit(uint32_t fan_index);
		GpuTelemetryCapBits GetPsuTelemetryCapBit(uint32_t psu_index);
		// data
		ctl_device_adapter_handle_t deviceHandle = nullptr;
		LUID deviceId; // pointed to by a device_adapter_properties member, written to by igcl api
		ctl_device_adapter_properties_t properties{};
		std::vector<ctl_mem_handle_t> memoryModules;
		mutable std::mutex historyMutex;
		TelemetryHistory<PresentMonPowerTelemetryInfo> history{ PowerTelemetryAdapter::defaultHistorySize };
		std::optional<ctl_power_telemetry_t> previousSample;
		std::optional<ctl_mem_bandwidth_t> previousMemBwSample;
		double time_delta_ = 0.f;
	};
}