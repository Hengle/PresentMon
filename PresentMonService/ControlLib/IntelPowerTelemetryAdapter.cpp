// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "IntelPowerTelemetryAdapter.h"
#include "Logging.h"

namespace pwr::intel
{
    // public interface functions

    IntelPowerTelemetryAdapter::IntelPowerTelemetryAdapter(ctl_device_adapter_handle_t handle)
        :
        deviceHandle{ handle }
    {
        properties = {
            .Size = sizeof(ctl_device_adapter_properties_t),
            .pDeviceID = &deviceId,
            .device_id_size = sizeof(deviceId),
        };

        if (auto result = ctlGetDeviceProperties(deviceHandle, &properties);
            result != CTL_RESULT_SUCCESS)
        {
            throw std::runtime_error{ "Failure to get device properties" };
        }

        if (properties.device_type != CTL_DEVICE_TYPE_GRAPHICS) {
            throw NonGraphicsDeviceException{};
        }

        if (auto result = EnumerateMemoryModules(); result != CTL_RESULT_SUCCESS) {
            throw std::runtime_error{ "Failed to enumerate memory modules" };
        }
    }

    bool IntelPowerTelemetryAdapter::Sample() noexcept
    {
        LARGE_INTEGER qpc;
        QueryPerformanceCounter(&qpc);
        bool success = true;

        ctl_power_telemetry_t currentSample{};
        currentSample.Size = sizeof(ctl_power_telemetry_t);
        if (const auto result = ctlPowerTelemetryGet(deviceHandle,
            &currentSample); result != CTL_RESULT_SUCCESS)
        {
            success = false;
            IGCL_ERR(result);
        }

        ctl_mem_state_t memory_state = {};
        memory_state.Size = sizeof(ctl_mem_state_t);
        if (const auto result = ctlMemoryGetState(
            memoryModules[0],
            &memory_state); result != CTL_RESULT_SUCCESS)
        {
            success = false;
            IGCL_ERR(result);
        }

        ctl_mem_bandwidth_t memory_bandwidth = {
            .Size = sizeof(ctl_mem_bandwidth_t),
            .Version = 1,
        };
        if (const auto result = ctlMemoryGetBandwidth(
            memoryModules[0],
            &memory_bandwidth); result != CTL_RESULT_SUCCESS)
        {
            success = false;
            IGCL_ERR(result);
        }
        
        if (const auto result = GetTimeDelta(currentSample);
            result != CTL_RESULT_SUCCESS)
        {
            success = false;
            IGCL_ERR(result);
        }

        double gpu_sustained_power_limit_mw = 0.;
        if (const auto result = ctlOverclockPowerLimitGet(
                deviceHandle, &gpu_sustained_power_limit_mw);
            result != CTL_RESULT_SUCCESS) {
            success = false;
            IGCL_ERR(result);
        }

        PresentMonPowerTelemetryInfo pm_gpu_power_telemetry_info{ .qpc = (uint64_t)qpc.QuadPart };

        if (previousSample) {

            if (const auto result = GetGPUPowerTelemetryData(
                currentSample, pm_gpu_power_telemetry_info); result != CTL_RESULT_SUCCESS)
            {
                success = false;
                IGCL_ERR(result);
            }

            if (const auto result = GetVramPowerTelemetryData(
                currentSample, pm_gpu_power_telemetry_info); result != CTL_RESULT_SUCCESS)
            {
                success = false;
                IGCL_ERR(result);
            }

            if (const auto result = GetFanPowerTelemetryData(currentSample,
                pm_gpu_power_telemetry_info); result != CTL_RESULT_SUCCESS)
            {
                success = false;
                IGCL_ERR(result);
            }

            if (const auto result = GetPsuPowerTelemetryData(
                currentSample, pm_gpu_power_telemetry_info); result != CTL_RESULT_SUCCESS)
            {
                success = false;
                IGCL_ERR(result);
            }

            // Get memory state and bandwidth data
            GetMemStateTelemetryData(memory_state, pm_gpu_power_telemetry_info);
            GetMemBandwidthData(memory_bandwidth, pm_gpu_power_telemetry_info);

            // Save and convert the gpu sustained power limit
            pm_gpu_power_telemetry_info.gpu_sustained_power_limit_w =
                gpu_sustained_power_limit_mw / 1000.;

            // Save off the calculated PresentMon power telemetry values. These are
            // saved off for clients to extrace out timing information based on QPC
            SavePmPowerTelemetryData(pm_gpu_power_telemetry_info);
        }

        // Save off the raw control library data for calculating time delta
        // and usage data.
        if (const auto result = SaveTelemetry(currentSample, memory_bandwidth);
            result != CTL_RESULT_SUCCESS)
        {
            success = false;
            IGCL_ERR(result);
        }

        return success;
    }

    std::optional<PresentMonPowerTelemetryInfo> IntelPowerTelemetryAdapter::GetClosest(uint64_t qpc) const noexcept
    {
        std::lock_guard<std::mutex> lock(historyMutex);
        return history.GetNearest(qpc);
    }

    PM_GPU_VENDOR IntelPowerTelemetryAdapter::GetVendor() const noexcept
    {
        return PM_GPU_VENDOR::PM_GPU_VENDOR_INTEL;
    }

    std::string IntelPowerTelemetryAdapter::GetName() const noexcept
    {
        return properties.name;
    }


    // private implementation functions

    ctl_result_t IntelPowerTelemetryAdapter::EnumerateMemoryModules()
    {
        // first call ctlEnumMemoryModules with nullptr to get number of modules
        // and resize vector to accomodate
        uint32_t memory_module_count = 0;
        {
            if (auto result = ctlEnumMemoryModules(deviceHandle, &memory_module_count,
                nullptr); result != CTL_RESULT_SUCCESS)
            {
                return result;
            }
            memoryModules.resize(size_t(memory_module_count));
        }

        if (auto result = ctlEnumMemoryModules(deviceHandle, &memory_module_count,
            memoryModules.data()); result != CTL_RESULT_SUCCESS)
        {
            return result;
        }

        return CTL_RESULT_SUCCESS;
    }

    // TODO: stop using CTL stuff for non-ctl logic
    // TODO: better functional programming
    ctl_result_t IntelPowerTelemetryAdapter::GetTimeDelta(const ctl_power_telemetry_t& currentSample)
    {
        if (!previousSample) {
            // We do not have a previous power telemetry item to calculate time
            // delta against.
            time_delta_ = 0.f;
        }
        else {
            if (currentSample.timeStamp.type == CTL_DATA_TYPE_DOUBLE) {
                time_delta_ = currentSample.timeStamp.value.datadouble -
                    previousSample->timeStamp.value.datadouble;
            }
            else {
                return CTL_RESULT_ERROR_INVALID_ARGUMENT;
            }
        }

        return CTL_RESULT_SUCCESS;
    }

    ctl_result_t IntelPowerTelemetryAdapter::GetGPUPowerTelemetryData(
        const ctl_power_telemetry_t& currentSample,
        PresentMonPowerTelemetryInfo& pm_gpu_power_telemetry_info)
    {
        ctl_result_t result;

        if (!previousSample) {
            return CTL_RESULT_ERROR_INVALID_ARGUMENT;
        }

        result = GetInstantaneousPowerTelemetryItem(
            currentSample.timeStamp,
            pm_gpu_power_telemetry_info.time_stamp);
        if (result != CTL_RESULT_SUCCESS) {
            return result;
        }

        result = GetInstantaneousPowerTelemetryItem(
            currentSample.gpuVoltage,
            pm_gpu_power_telemetry_info.gpu_voltage_v);
        if (result != CTL_RESULT_SUCCESS) {
            return result;
        }

        result = GetInstantaneousPowerTelemetryItem(
            currentSample.gpuCurrentClockFrequency,
            pm_gpu_power_telemetry_info.gpu_frequency_mhz);
        if (result != CTL_RESULT_SUCCESS) {
            return result;
        }

        result = GetInstantaneousPowerTelemetryItem(
            currentSample.gpuCurrentTemperature,
            pm_gpu_power_telemetry_info.gpu_temperature_c);
        if (result != CTL_RESULT_SUCCESS) {
            return result;
        }

        result = GetPowerTelemetryItemUsage(
            currentSample.gpuEnergyCounter,
            previousSample->gpuEnergyCounter,
            pm_gpu_power_telemetry_info.gpu_power_w);
        if (result != CTL_RESULT_SUCCESS) {
            return result;
        }

        result = GetPowerTelemetryItemUsagePercent(
            currentSample.globalActivityCounter,
            previousSample->globalActivityCounter,
            pm_gpu_power_telemetry_info.gpu_utilization);
        if (result != CTL_RESULT_SUCCESS) {
            return result;
        }

        result = GetPowerTelemetryItemUsagePercent(
            currentSample.globalActivityCounter,
            previousSample->globalActivityCounter,
            pm_gpu_power_telemetry_info.gpu_utilization);
        if (result != CTL_RESULT_SUCCESS) {
            return result;
        }

        result = GetPowerTelemetryItemUsagePercent(
            currentSample.renderComputeActivityCounter,
            previousSample->renderComputeActivityCounter,
            pm_gpu_power_telemetry_info.gpu_render_compute_utilization);
        if (result != CTL_RESULT_SUCCESS) {
            return result;
        }

        result = GetPowerTelemetryItemUsagePercent(
            currentSample.mediaActivityCounter,
            previousSample->mediaActivityCounter,
            pm_gpu_power_telemetry_info.gpu_media_utilization);
        if (result != CTL_RESULT_SUCCESS) {
            return result;
        }

        pm_gpu_power_telemetry_info.gpu_power_limited =
            currentSample.gpuPowerLimited;
        pm_gpu_power_telemetry_info.gpu_temperature_limited =
            currentSample.gpuTemperatureLimited;
        pm_gpu_power_telemetry_info.gpu_current_limited =
            currentSample.gpuCurrentLimited;
        pm_gpu_power_telemetry_info.gpu_voltage_limited =
            currentSample.gpuVoltageLimited;
        pm_gpu_power_telemetry_info.gpu_utilization_limited =
            currentSample.gpuUtilizationLimited;

        return result;
    }

    ctl_result_t IntelPowerTelemetryAdapter::GetVramPowerTelemetryData(
        const ctl_power_telemetry_t& currentSample,
        PresentMonPowerTelemetryInfo& pm_gpu_power_telemetry_info)
    {
        ctl_result_t result;

        if (!previousSample) {
            return CTL_RESULT_ERROR_INVALID_ARGUMENT;
        }

        result = GetInstantaneousPowerTelemetryItem(currentSample.vramVoltage,
            pm_gpu_power_telemetry_info.vram_voltage_v);
        if (result != CTL_RESULT_SUCCESS) {
            return result;
        }

        result = GetInstantaneousPowerTelemetryItem(
            currentSample.vramCurrentClockFrequency,
            pm_gpu_power_telemetry_info.vram_frequency_mhz);
        if (result != CTL_RESULT_SUCCESS) {
            return result;
        }

        result = GetInstantaneousPowerTelemetryItem(
            currentSample.vramCurrentEffectiveFrequency,
            pm_gpu_power_telemetry_info.vram_effective_frequency_gbps);
        if (result != CTL_RESULT_SUCCESS) {
            return result;
        }

        result = GetInstantaneousPowerTelemetryItem(
            currentSample.vramCurrentTemperature,
            pm_gpu_power_telemetry_info.vram_temperature_c);
        if (result != CTL_RESULT_SUCCESS) {
            return result;
        }

        result = GetPowerTelemetryItemUsage(
            currentSample.vramReadBandwidthCounter,
            previousSample->vramReadBandwidthCounter,
            pm_gpu_power_telemetry_info.vram_read_bandwidth_bps);
        if (result != CTL_RESULT_SUCCESS) {
          return result;
        }

        result = GetPowerTelemetryItemUsage(
            currentSample.vramWriteBandwidthCounter,
            previousSample->vramWriteBandwidthCounter,
            pm_gpu_power_telemetry_info.vram_write_bandwidth_bps);
        if (result != CTL_RESULT_SUCCESS) {
          return result;
        }

        result = GetPowerTelemetryItemUsage(currentSample.vramEnergyCounter,
            previousSample->vramEnergyCounter,
            pm_gpu_power_telemetry_info.vram_power_w);
        if (result != CTL_RESULT_SUCCESS) {
            return result;
        }

        pm_gpu_power_telemetry_info.vram_power_limited =
            currentSample.vramPowerLimited;
        pm_gpu_power_telemetry_info.vram_current_limited =
            currentSample.vramCurrentLimited;
        pm_gpu_power_telemetry_info.vram_voltage_limited =
            currentSample.vramVoltageLimited;
        pm_gpu_power_telemetry_info.vram_utilization_limited =
            currentSample.vramUtilizationLimited;

        return result;
    }

    ctl_result_t IntelPowerTelemetryAdapter::GetFanPowerTelemetryData(
        const ctl_power_telemetry_t& currentSample,
        PresentMonPowerTelemetryInfo& pm_gpu_power_telemetry_info)
    {
        ctl_result_t result = CTL_RESULT_SUCCESS;

        for (uint32_t i = 0; i < CTL_FAN_COUNT; i++) {
            result = GetInstantaneousPowerTelemetryItem(
                currentSample.fanSpeed[i],
                pm_gpu_power_telemetry_info.fan_speed_rpm[i]);
            if (result != CTL_RESULT_SUCCESS) {
                return result;
            }
        }

        return result;
    }

    ctl_result_t IntelPowerTelemetryAdapter::GetPsuPowerTelemetryData(
        const ctl_power_telemetry_t& currentSample,
        PresentMonPowerTelemetryInfo& pm_gpu_power_telemetry_info)
    {
        ctl_result_t result = CTL_RESULT_SUCCESS;

        if (!previousSample) {
            return CTL_RESULT_ERROR_INVALID_ARGUMENT;
        }

        for (uint32_t i = 0; i < CTL_PSU_COUNT; i++) {
            if (currentSample.psu[i].bSupported) {
                pm_gpu_power_telemetry_info.psu[i].psu_type =
                    PresentMonPsuType(currentSample.psu[i].psuType);
                result = GetInstantaneousPowerTelemetryItem(
                    currentSample.psu[i].voltage,
                    pm_gpu_power_telemetry_info.psu[i].psu_voltage);
                if (result != CTL_RESULT_SUCCESS) {
                    return result;
                }
                result = GetPowerTelemetryItemUsage(
                    currentSample.psu[i].energyCounter,
                    previousSample->psu[i].energyCounter,
                    pm_gpu_power_telemetry_info.psu[i].psu_power);
                if (result != CTL_RESULT_SUCCESS) {
                    return result;
                }
            }
            else {
                pm_gpu_power_telemetry_info.psu[i].psu_type =
                    PresentMonPsuType::None;
                pm_gpu_power_telemetry_info.psu[i].psu_power = 0.0f;
                pm_gpu_power_telemetry_info.psu[i].psu_voltage = 0.0f;
            }
        }

        return result;
    }

    void IntelPowerTelemetryAdapter::GetMemStateTelemetryData(
        const ctl_mem_state_t& mem_state,
        PresentMonPowerTelemetryInfo& pm_gpu_power_telemetry_info) {
        pm_gpu_power_telemetry_info.gpu_mem_total_size_b = mem_state.size;
        pm_gpu_power_telemetry_info.gpu_mem_used_b = mem_state.size - mem_state.free;
        return;
    }

    void IntelPowerTelemetryAdapter::GetMemBandwidthData(
        const ctl_mem_bandwidth_t& mem_bandwidth,
        PresentMonPowerTelemetryInfo& pm_gpu_power_telemetry_info) {
        pm_gpu_power_telemetry_info.gpu_mem_max_bandwidth_bps = mem_bandwidth.maxBandwidth;

        pm_gpu_power_telemetry_info.gpu_mem_read_bandwidth_bps = 0.f;
        pm_gpu_power_telemetry_info.gpu_mem_write_bandwidth_bps = 0.f;
        if (previousMemBwSample) {
            if(mem_bandwidth.Version > 0) {
                double delta_time_secs = 
                    (mem_bandwidth.timestamp - previousMemBwSample->timestamp) / 
                    1e6;
                if (delta_time_secs != 0.f) {
                    pm_gpu_power_telemetry_info.gpu_mem_read_bandwidth_bps = 
                        static_cast<double>(mem_bandwidth.readCounter - 
                            previousMemBwSample->readCounter) / delta_time_secs;
                    pm_gpu_power_telemetry_info.gpu_mem_write_bandwidth_bps = 
                        static_cast<double>(mem_bandwidth.writeCounter - 
                            previousMemBwSample->writeCounter) / delta_time_secs;
                }
            }
        }
        return;
    }
    
    ctl_result_t IntelPowerTelemetryAdapter::GetInstantaneousPowerTelemetryItem(
        const ctl_oc_telemetry_item_t& telemetry_item,
        double& pm_telemetry_value)
    {
        if (telemetry_item.bSupported) {
            if (telemetry_item.type == CTL_DATA_TYPE_DOUBLE) {
                pm_telemetry_value = telemetry_item.value.datadouble;
            }
            else {
                // Expecting a double return type here
                return CTL_RESULT_ERROR_INVALID_ARGUMENT;
            }
        }
        else {
            pm_telemetry_value = 0.0;
        }
        return CTL_RESULT_SUCCESS;
    }

    ctl_result_t IntelPowerTelemetryAdapter::GetPowerTelemetryItemUsagePercent(
        const ctl_oc_telemetry_item_t& current_telemetry_item,
        const ctl_oc_telemetry_item_t& previous_telemetry_item,
        double& pm_telemetry_value)
    {
        if (current_telemetry_item.bSupported) {
            if (current_telemetry_item.type == CTL_DATA_TYPE_DOUBLE) {
                auto data_delta = current_telemetry_item.value.datadouble -
                    previous_telemetry_item.value.datadouble;
                pm_telemetry_value = (data_delta / time_delta_) * 100.0f;
            }
            else {
                // Expecting a double return type here
                return CTL_RESULT_ERROR_INVALID_ARGUMENT;
            }
        }
        return CTL_RESULT_SUCCESS;
    }

    ctl_result_t IntelPowerTelemetryAdapter::GetPowerTelemetryItemUsage(
        const ctl_oc_telemetry_item_t& current_telemetry_item,
        const ctl_oc_telemetry_item_t& previous_telemetry_item,
        double& pm_telemetry_value)
    {
        if (current_telemetry_item.bSupported) {
            if (current_telemetry_item.type == CTL_DATA_TYPE_DOUBLE) {
                auto data_delta = current_telemetry_item.value.datadouble -
                    previous_telemetry_item.value.datadouble;
                pm_telemetry_value = (data_delta / time_delta_);
            } 
            else if (current_telemetry_item.type == CTL_DATA_TYPE_INT64) {
                auto data_delta = current_telemetry_item.value.data64 -
                    previous_telemetry_item.value.data64;
                pm_telemetry_value = static_cast<double>(data_delta) / time_delta_;
            }
            else if (current_telemetry_item.type == CTL_DATA_TYPE_UINT64) {
              auto data_delta = current_telemetry_item.value.datau64 -
                                previous_telemetry_item.value.datau64;
              pm_telemetry_value =
                  static_cast<double>(data_delta) / time_delta_;
            }
            else {
                // Expecting a double return type here
                return CTL_RESULT_ERROR_INVALID_ARGUMENT;
            }
        }
        return CTL_RESULT_SUCCESS;
    }

    ctl_result_t IntelPowerTelemetryAdapter::SaveTelemetry(
        const ctl_power_telemetry_t& currentSample,
        const ctl_mem_bandwidth_t& currentMemBandwidthSample)
    {
        if (currentSample.timeStamp.type == CTL_DATA_TYPE_DOUBLE) {
            previousSample = currentSample;
        }
        else {
            return CTL_RESULT_ERROR_INVALID_ARGUMENT;
        }

        previousMemBwSample = currentMemBandwidthSample;

        return CTL_RESULT_SUCCESS;
    }

    void IntelPowerTelemetryAdapter::SavePmPowerTelemetryData(PresentMonPowerTelemetryInfo& info)
    {
        std::lock_guard<std::mutex> lock(historyMutex);
        history.Push(info);
    }
}