// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "RawFrameDataWriter.h"
#include "adapt/RawAdapter.h"
#include "PresentMode.h"
#include <Core/source/infra/util/Util.h>
#include <Core/source/infra/util/Assert.h>

namespace p2c::pmon
{
    RawFrameDataWriter::RawFrameDataWriter(std::wstring path, adapt::RawAdapter* pAdapter, std::optional<std::wstring> frameStatsPathIn)
        :
        pAdapter{ pAdapter },
        frameStatsPath{ std::move(frameStatsPathIn) },
        pStatsTracker{ frameStatsPath ? std::make_unique<StatisticsTracker>() : nullptr },
        file{ path }
    {
        // write header
        file <<
            "Application,"
            "ProcessID,"
            "SwapChainAddress,"
            "Runtime,"
            "SyncInterval,"
            "PresentFlags,"
            "Dropped,"
            "TimeInSeconds,"
            "msInPresentAPI,"
            "msBetweenPresents,"
            "AllowsTearing,"
            "PresentMode,"
            "msUntilRenderComplete,"
            "msUntilDisplayed,"
            "msBetweenDisplayChange,"
            "msUntilRenderStart,"
            "msGPUActive,"
            "msGPUVideoActive,"
            "msSinceInput,"
            "QPCtime,"
            "GPUPower[W],"
            "GPUSustainedPowerLimit[W],"
            "GPUVoltage[V],"
            "GPUFrequency[MHz],"
            "GPUTemperature[C],"
            "GPUUtilization[%],"
            "GPURenderComputeUtilization[%],"
            "GPUMediaUtilization[%],"
            "VRAMPower[W],"
            "VRAMVoltage[V],"
            "VRAMFrequency[Mhz],"
            "VRAMEffectiveFrequency[GBps],"
            "VRAMTemperature[C],"
            "GPUMemTotalSize[B],"
            "GPUMemUsed[B],"
            "GPUMemMaxBandwidth[GBps],"
            "GPUMemReadBandwidth[Bps],"
            "GPUMemWriteBandwidth[Bps],"
            "GPUFanSpeed0[RPM],"
            "GPUFanSpeed1[RPM],"
            "GPUFanSpeed2[RPM],"
            "GPUFanSpeed3[RPM],"
            "GPUFanSpeed4[RPM],"
            "PSUType0,"
            "PSUType1,"
            "PSUType2,"
            "PSUType3,"
            "PSUType4,"
            "PSUPower0[W],"
            "PSUPower1[W],"
            "PSUPower2[W],"
            "PSUPower3[W],"
            "PSUPower4[W],"
            "PSUVoltage0[V],"
            "PSUVoltage1[V],"
            "PSUVoltage2[V],"
            "PSUVoltage3[V],"
            "PSUVoltage4[V],"
            "GPUPowerLimited,"
            "GPUTemperatureLimited,"
            "GPUCurrentLimited,"
            "GPUVoltageLimited,"
            "GPUUtilizationLimited,"
            "VRAMPowerLimited,"
            "VRAMTemperatureLimited,"
            "VRAMCurrentLimited,"
            "VRAMVoltageLimited,"
            "VRAMUtilizationLimited,"
            "CPUUtilization[%],"
            "CPUFrequency[MHz],"
            "CPUPower[W],"
            "CPUPowerLimit[W],"
            "CPUTemperature[C]\n";
    }

    void RawFrameDataWriter::Process(double timestamp)
    {
        for (auto& f : pAdapter->Pull(timestamp))
        {
            if (pStatsTracker) {
                // tracking trace duration
                if (startTime < 0.) {
                    startTime = f.time_in_seconds;
                    endTime = f.time_in_seconds;
                }
                else {
                    endTime = f.time_in_seconds;
                }
                // tracking frame times
                pStatsTracker->Push(f.ms_between_presents);
            }

            file
                << f.application << ","
                << f.process_id << ","
                << f.swap_chain_address << ","
                << f.runtime << ","
                << f.sync_interval << ","
                << f.present_flags << ","
                << f.dropped << ","
                << f.time_in_seconds << ","
                << f.ms_in_present_api << ","
                << f.ms_between_presents << ","
                << f.allows_tearing << ","
                << infra::util::ToNarrow(PresentModeToString(ConvertPresentMode((PM_PRESENT_MODE)f.present_mode))) << ","
                << f.ms_until_render_complete << ","
                << f.ms_until_displayed << ","
                << f.ms_between_display_change << ","
                << f.ms_until_render_start << ","
                << f.ms_gpu_active << ","
                << f.ms_gpu_video_active << ","
                << f.ms_since_input << ","
                << f.qpc_time << ","
                << f.gpu_power_w << ","
                << f.gpu_sustained_power_limit_w << ","
                << f.gpu_voltage_v << ","
                << f.gpu_frequency_mhz << ","
                << f.gpu_temperature_c << ","
                << f.gpu_utilization << ","
                << f.gpu_render_compute_utilization << ","
                << f.gpu_media_utilization << ","
                << f.vram_power_w << ","
                << f.vram_voltage_v << ","
                << f.vram_frequency_mhz << ","
                << f.vram_effective_frequency_gbs << ","
                << f.vram_temperature_c << ","
                << f.gpu_mem_total_size_b << ","
                << f.gpu_mem_used_b << ","
                << f.gpu_mem_max_bandwidth_bps << ","
                << f.gpu_mem_read_bandwidth_bps << ","
                << f.gpu_mem_write_bandwidth_bps << ","
                << f.fan_speed_rpm[0] << ","
                << f.fan_speed_rpm[1] << ","
                << f.fan_speed_rpm[2] << ","
                << f.fan_speed_rpm[3] << ","
                << f.fan_speed_rpm[4] << ","
                << f.psu_type[0] << ","
                << f.psu_type[1] << ","
                << f.psu_type[2] << ","
                << f.psu_type[3] << ","
                << f.psu_type[4] << ","
                << f.psu_power[0] << ","
                << f.psu_power[1] << ","
                << f.psu_power[2] << ","
                << f.psu_power[3] << ","
                << f.psu_power[4] << ","
                << f.psu_voltage[0] << ","
                << f.psu_voltage[1] << ","
                << f.psu_voltage[2] << ","
                << f.psu_voltage[3] << ","
                << f.psu_voltage[4] << ","
                << f.gpu_power_limited << ","
                << f.gpu_temperature_limited << ","
                << f.gpu_current_limited << ","
                << f.gpu_voltage_limited << ","
                << f.gpu_utilization_limited << ","
                << f.vram_power_limited << ","
                << f.vram_temperature_limited << ","
                << f.vram_current_limited << ","
                << f.vram_voltage_limited << ","
                << f.vram_utilization_limited << ","
                << f.cpu_utilization << ","
                << f.cpu_frequency << ","
                << f.cpu_power_w << ","
                << f.cpu_power_limit_w << ","
                << f.cpu_temperature_c << "\n";
        }
    }

    double RawFrameDataWriter::GetDuration_() const
    {
        return endTime - startTime;
    }

    void RawFrameDataWriter::WriteStats_()
    {
        auto& stats = *pStatsTracker;

        std::ofstream statsFile{ *frameStatsPath, std::ios::trunc };

        // write header
        statsFile <<
            "Duration,"
            "Total Frames,"
            "Average FPS,"
            "Minimum FPS,"
            "99th Percentile FPS,"
            "95th Percentile FPS,"
            "Maximum FPS\n";

        // lambda to make sure we don't divide by zero
        // caps max fps output to 1,000,000 fps
        const auto SafeInvert = [](double ft) {
            return ft == 0. ? 1'000'000. : 1. / ft;
		};

        if (stats.GetCount() > 0) {
            // write data
            statsFile <<
                GetDuration_() << "," <<
                stats.GetCount() << "," <<
                SafeInvert(stats.GetMean()) << "," <<
                SafeInvert(stats.GetMax()) << "," <<
                SafeInvert(stats.GetPercentile(.99)) << "," <<
                SafeInvert(stats.GetPercentile(.95)) << "," <<
                SafeInvert(stats.GetMin()) << "\n";
        }
        else {
			// write null data
			statsFile <<
				0. << "," <<
				0. << "," <<
				0. << "," <<
				0. << "," <<
				0. << "," <<
				0. << "," <<
				0. << "\n";
        }
    }

    RawFrameDataWriter::~RawFrameDataWriter()
    {
        try {
            if (pStatsTracker) {
                WriteStats_();
            }
        }
        catch (...) {}
    }
}