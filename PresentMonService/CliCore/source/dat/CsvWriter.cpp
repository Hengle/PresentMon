// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "CsvWriter.h"
#include <CliCore/source/pmon/PresentMode.h>
#include <Core/source/infra/util/Util.h>
#include <iostream>
#include <set>


#define COLUMN_LIST \
X_("Application", "", application,,, core) \
X_("ProcessID", "", process_id,,, core) \
X_("SwapChainAddress", "", swap_chain_address,,, core) \
X_("Runtime", "", runtime,,, core) \
X_("SyncInterval", "", sync_interval,,, core) \
X_("PresentFlags", "", present_flags,,, core) \
X_("Dropped", "", dropped,,, core) \
X_("Time", "[s]", time_in_seconds,,, core) \
X_("InPresentAPI", "[ms]", ms_in_present_api,,, core) \
X_("BetweenPresents", "[ms]", ms_between_presents,,, core) \
X_("AllowsTearing", "", allows_tearing,,, core) \
X_("PresentMode", "", present_mode, TransformPresentMode,, core) \
X_("UntilRenderComplete", "[ms]", ms_until_render_complete,,, core) \
X_("UntilDisplayed", "[ms]", ms_until_displayed,,, core) \
X_("BetweenDisplayChange", "[ms]", ms_between_display_change,,, core) \
X_("UntilRenderStart", "[ms]", ms_until_render_start,,, core) \
X_("QpcTime", "", qpc_time,,, core) \
X_("SinceInput", "[ms]", ms_since_input,,, input) \
X_("StalledQueueFull", "[ms]", ms_stalled_on_queue_full,,, blocking) \
X_("WaitingQueueSync", "[ms]", ms_waiting_on_queue_sync,,, blocking) \
X_("WaitingQueueDrain", "[ms]", ms_waiting_on_queue_drain,,, blocking) \
X_("WaitingFence", "[ms]", ms_waiting_on_fence,,, blocking) \
X_("WaitingFenceSubmission", "[ms]", ms_waiting_on_fence_submission,,, blocking) \
X_("StalledQueueEmpty", "[ms]", ms_stalled_on_queue_empty,,, blocking) \
X_("BetweenProducerPresents", "[ms]", ms_between_producer_presents,,, blocking) \
X_("BetweenConsumerEvents", "[ms]", ms_between_consumer_presents,,, blocking) \
X_("GPUActive", "[ms]", ms_gpu_active,,, activity) \
X_("GPUVideoActive", "[ms]", ms_gpu_video_active,,, activity) \
X_("WaitingSyncObject", "[ms]", ms_waiting_on_sync_object,,, blocking) \
X_("WaitingQueryData", "[ms]", ms_waiting_on_query_data,,, blocking) \
X_("WaitingDrawTimeCompilation", "[ms]", ms_waiting_on_draw_time_compilation,,, compile) \
X_("WaitingCreateTimeCompilation", "[ms]", ms_waiting_on_create_time_compilation,,, compile) \
X_("InMakeResident", "[ms]", ms_in_make_resident,,, residency) \
X_("InPagingPackets", "[ms]", ms_in_paging_packets,,, residency) \
X_("GPUPower", "[W]", gpu_power_w,,, gputele) \
X_("GPUSustainedPowerLimit", "[W]", gpu_sustained_power_limit_w,,, gputele) \
X_("GPUVoltage", "[V]", gpu_voltage_v,,, gputele) \
X_("GPUFrequency", "[MHz]", gpu_frequency_mhz,,, gputele) \
X_("GPUTemperature", "[C]", gpu_temperature_c,,, gputele) \
X_("GPUUtilization", "[%]", gpu_utilization,,, gputele) \
X_("GPURenderComputeUtilization", "[%]", gpu_render_compute_utilization,,, gputele) \
X_("GPUMediaUtilization", "[%]", gpu_media_utilization,,, gputele) \
X_("VRAMPower", "[W]", vram_power_w,,, vramtele) \
X_("VRAMVoltage", "[V]", vram_voltage_v,,, vramtele) \
X_("VRAMFrequency", "[MHz]", vram_frequency_mhz,,, vramtele) \
X_("VRAMEffectiveFrequency", "[Gbps]", vram_effective_frequency_gbs,,, vramtele) \
X_("VRAMReadBandwidth", "[bps]", vram_read_bandwidth_bps,,, vramtele) \
X_("VRAMWriteBandwidth", "[bps]", vram_write_bandwidth_bps,,, vramtele) \
X_("VRAMTemperature", "[C]", vram_temperature_c,,, vramtele) \
X_("GPUFanSpeed0", "[rpm]", fan_speed_rpm,, [0], fan) \
X_("GPUFanSpeed1", "[rpm]", fan_speed_rpm,, [1], fan) \
X_("GPUFanSpeed2", "[rpm]", fan_speed_rpm,, [2], fan) \
X_("GPUFanSpeed3", "[rpm]", fan_speed_rpm,, [3], fan) \
X_("GPUFanSpeed4", "[rpm]", fan_speed_rpm,, [4], fan) \
X_("PSUType0", "", psu_type,, [0], psu) \
X_("PSUType1", "", psu_type,, [1], psu) \
X_("PSUType2", "", psu_type,, [2], psu) \
X_("PSUType3", "", psu_type,, [3], psu) \
X_("PSUType4", "", psu_type,, [4], psu) \
X_("PSUPower0", "[W]", psu_power,, [0], psu) \
X_("PSUPower1", "[W]", psu_power,, [1], psu) \
X_("PSUPower2", "[W]", psu_power,, [2], psu) \
X_("PSUPower3", "[W]", psu_power,, [3], psu) \
X_("PSUPower4", "[W]", psu_power,, [4], psu) \
X_("PSUPower0", "[V]", psu_voltage,, [0], psu) \
X_("PSUPower1", "[V]", psu_voltage,, [1], psu) \
X_("PSUPower2", "[V]", psu_voltage,, [2], psu) \
X_("PSUPower3", "[V]", psu_voltage,, [3], psu) \
X_("PSUPower4", "[V]", psu_voltage,, [4], psu) \
X_("GPUMemTotalSize", "[B]", gpu_mem_total_size_b,,, gpumemtele) \
X_("GPUMemUsed", "[B]", gpu_mem_used_b,,, gpumemtele) \
X_("GPUMemMaxBandwidth", "[bps]", gpu_mem_max_bandwidth_bps,,, gpumemtele) \
X_("GPUMemReadBandwidth", "[bps]", gpu_mem_read_bandwidth_bps,,, gpumemtele) \
X_("GPUMemWriteBandwidth", "[bps]", gpu_mem_write_bandwidth_bps,,, gpumemtele) \
X_("GPUPowerLimited", "", gpu_power_limited,,, throttle) \
X_("GPUTemperatureLimited", "", gpu_temperature_limited,,, throttle) \
X_("GPUCurrentLimited", "", gpu_current_limited,,, throttle) \
X_("GPUVoltageLimited", "", gpu_voltage_limited,,, throttle) \
X_("GPUUtilizationLimited", "", gpu_utilization_limited,,, throttle) \
X_("VRAMPowerLimited", "", vram_power_limited,,, throttle) \
X_("VRAMTemperatureLimited", "", vram_temperature_limited,,, throttle) \
X_("VRAMCurrentLimited", "", vram_current_limited,,, throttle) \
X_("VRAMVoltageLimited", "", vram_voltage_limited,,, throttle) \
X_("VRAMUtilizationLimited", "", vram_utilization_limited,,, throttle) \
X_("CPUUtilization", "[%]", cpu_utilization,,, cputele) \
X_("CPUPower", "[W]", cpu_power_w,,, cputele) \
X_("CPUPowerLimit", "[W]", cpu_power_limit_w,,, cputele) \
X_("CPUTemperature", "[C]", cpu_temperature_c,,, cputele) \
X_("CPUFrequency", "[GHz]", cpu_frequency,,, cputele) \
X_("CPUBias", "", cpu_bias,,, bias) \
X_("GPUBias", "", gpu_bias,,, bias)

#define COLUMN_GROUP_LIST \
X_(core) \
X_(input) \
X_(activity) \
X_(blocking) \
X_(compile) \
X_(residency) \
X_(gputele) \
X_(vramtele) \
X_(fan) \
X_(psu) \
X_(throttle) \
X_(gpumemtele) \
X_(cputele) \
X_(bias)

namespace p2c::cli::dat
{
    struct GroupFlags
    {
#define X_(name) bool name;
        COLUMN_GROUP_LIST
#undef X_
    };

    CsvWriter::~CsvWriter() = default;
    CsvWriter::CsvWriter(CsvWriter&&) = default;
    CsvWriter& CsvWriter::operator=(CsvWriter&&) = default;

    CsvWriter::CsvWriter(std::string path, const std::vector<std::string>& groups, bool writeStdout)
        :
        writeStdout_{ writeStdout }
    {
        if (!path.empty()) {
            file_.open(path, std::ios::trunc);
            if (!file_) {
                throw std::runtime_error{ "Failed to open file for writing: " + path };
            }
        }

        // setup group flags
        std::set<std::string> groupSet{ groups.begin(), groups.end() };
        pGroupFlags_ = std::make_unique<GroupFlags>();
#define X_(name) pGroupFlags_->name = groupSet.contains("all") || groupSet.contains(#name);
        COLUMN_GROUP_LIST
#undef X_

        int col = 0;
        // write header
#define X_(name, unit, symbol, transform, index, group) if (pGroupFlags_->group) { if (col++) buffer_ << ',';  buffer_ << name unit; }
        COLUMN_LIST
#undef X_
        buffer_ << std::endl;

        Flush();
    }

    void CsvWriter::Process(const PM_FRAME_DATA& frame)
    {
        const auto TransformPresentMode = [](PM_PRESENT_MODE mode) {
            return infra::util::ToNarrow(pmon::PresentModeToString(pmon::ConvertPresentMode(mode)));
        };

        int col = 0;
#define X_(name, unit, symbol, transform, index, group) if (pGroupFlags_->group) { if (col++) buffer_ << ','; buffer_ << transform(frame.symbol index); }
        COLUMN_LIST
#undef X_
        buffer_ << std::endl;

        Flush();
    }

    void CsvWriter::Flush()
    {
        if (writeStdout_) {
            std::cout << buffer_.str();
        }
        if (file_) {
            file_ << buffer_.str();
        }
        buffer_.str({});
        buffer_.clear();
    }
}