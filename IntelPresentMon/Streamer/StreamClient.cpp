// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "StreamClient.h"
#include "../PresentMonUtils/QPCUtils.h"
#include "../PresentMonUtils/PresentDataUtils.h"

#define GOOGLE_GLOG_DLL_DECL
#define GLOG_NO_ABBREVIATED_SEVERITIES
#include <glog/logging.h>

StreamClient::StreamClient()
    : initialized_(false),
      next_dequeue_idx_(0),
      recording_frame_data_(false),
      current_dequeue_frame_num_(0),
      is_etl_stream_client_(false) {}

StreamClient::StreamClient(string mapfile_name, bool is_etl_stream_client)
    : next_dequeue_idx_(0),
      recording_frame_data_(false),
      current_dequeue_frame_num_(0),
      is_etl_stream_client_(is_etl_stream_client) {
  Initialize(std::move(mapfile_name));
}

StreamClient::~StreamClient() {
}

void StreamClient::Initialize(string mapfile_name) {
	shared_mem_view_ = std::make_unique<NamedSharedMem>();
	shared_mem_view_->OpenSharedMemView(mapfile_name);
	mapfile_name_ = std::move(mapfile_name);

	// Query qpc frequency
    try {
        LOG_IF(ERROR, !QueryPerformanceFrequency(&qpcFrequency_))
            << "QueryPerformanceFrequency failed with error: "
            << GetLastError();
    } catch (...) {
        LOG(ERROR) << "QueryPerformanceFrequency failed.";
    }

	initialized_ = true;
	LOG(INFO) << "Stream client initialized.";
}

void StreamClient::CloseSharedMemView() { shared_mem_view_.reset(nullptr); }

PmNsmFrameData* StreamClient::ReadLatestFrame() {
  PmNsmFrameData* data = nullptr;

  if (shared_mem_view_ == nullptr) {
    LOG(ERROR)
        << "Shared mem view is null. Initialze client with mapfile name.";
    return data;
  }

  if (shared_mem_view_->IsEmpty()) {
    LOG(INFO) << "Shared mem view is empty. start ETW tracing or wait for "
                 "more data";
    return data;
  }

  uint64_t index = GetLatestFrameIndex();

  return ReadFrameByIdx(index);
}

PmNsmFrameData* StreamClient::ReadFrameByIdx(uint64_t frame_id) {
  PmNsmFrameData* data = nullptr;
  uint64_t read_offset = 0;

  if (shared_mem_view_ == nullptr) {
    LOG(ERROR)
        << "Shared mem view is null. Initialze client with mapfile name.";
    return data;
  }

  if (shared_mem_view_->IsEmpty()) {
    return data;
  }

  auto p_header = shared_mem_view_->GetHeader();

  if (!p_header->process_active) {
    LOG(ERROR) << "Process is not active. Shared mem view to be destroyed.";
    CloseSharedMemView();
    return data;
  }

  if ((frame_id > p_header->max_entries - 1) ||
      ((frame_id > p_header->tail_idx) && (!shared_mem_view_->IsFull()))) {
    try {
      LOG(ERROR) << "Invalid frame_id: " << frame_id;
    } catch (...) {
      LOG(ERROR) << "Invalid frame.";
    }
    return data;
  }

  read_offset =
      frame_id * sizeof(PmNsmFrameData) + shared_mem_view_->GetBaseOffset();

  data = reinterpret_cast<PmNsmFrameData*>(
      static_cast<char*>((shared_mem_view_->GetBuffer())) + read_offset);

  return data;
}

// Record frames for online process monitoring. Reading from tail and copy data
// out. This function also keep track of drop frames in case client doesn't read frames fast enough.
PM_STATUS StreamClient::RecordFrame(PM_FRAME_DATA** out_frame_data) {
  if (is_etl_stream_client_) {
    LOG(INFO) << "ETL Client should be using DequeueFrame instead.";
    *out_frame_data = nullptr;
    return PM_STATUS::PM_STATUS_SERVICE_NOT_SUPPORTED;
  }

  PmNsmFrameData* data = nullptr;
  auto nsm_view = GetNamedSharedMemView();
  auto nsm_hdr = nsm_view->GetHeader();
  if (!nsm_hdr->process_active) {
    // Service destroyed the named shared memory.
    *out_frame_data = nullptr;
    return PM_STATUS::PM_STATUS_PROCESS_NOT_EXIST;
  }

  if (recording_frame_data_ == false) {
    // Get the current number of frames written and set it as the current
    // dequeue frame number. This will be used to track data overruns if
    // the client does not read data fast enough.
    recording_frame_data_ = true;
    current_dequeue_frame_num_ = nsm_hdr->num_frames_written;
    next_dequeue_idx_ = GetLatestFrameIndex();
  }

  // Check to see if the number of pending read frames is greater
  // than the maximum number of entries. If so we have lost frame
  // data
  uint64_t num_pending_frames = CheckPendingReadFrames();
  if (num_pending_frames > nsm_hdr->max_entries) {
    recording_frame_data_ = false;
    *out_frame_data = nullptr;
    return PM_STATUS::PM_STATUS_DATA_LOSS;
  } else if (num_pending_frames == 0) {
    *out_frame_data = nullptr;
    return PM_STATUS::PM_STATUS_NO_DATA;
  }

  data = ReadFrameByIdx(next_dequeue_idx_);
  if (data) {
    uint64_t max_entries = nsm_view->GetHeader()->max_entries;
    next_dequeue_idx_ = (next_dequeue_idx_ + 1) % max_entries;
    current_dequeue_frame_num_++;
    CopyFrameData(nsm_hdr->start_qpc, data, nsm_hdr->gpuTelemetryCapBits,
                  nsm_hdr->cpuTelemetryCapBits, *out_frame_data);

    return PM_STATUS::PM_STATUS_SUCCESS;
  } else {
    return PM_STATUS::PM_STATUS_ERROR;
  }
}

void StreamClient::CopyFrameData(uint64_t start_qpc,
                                 const PmNsmFrameData* src_frame,
                                 GpuTelemetryBitset gpu_telemetry_cap_bits,
                                 CpuTelemetryBitset cpu_telemetry_cap_bits,
                                 PM_FRAME_DATA* dst_frame) {
  memset(dst_frame, 0, sizeof(PM_FRAME_DATA));
  dst_frame->qpc_time = src_frame->present_event.PresentStartTime;

  dst_frame->ms_between_presents =
      QpcDeltaToMs(src_frame->present_event.PresentStartTime -
                       src_frame->present_event.last_present_qpc,
                   GetQpcFrequency());

  strcpy_s(dst_frame->application, src_frame->present_event.application);

  dst_frame->process_id = src_frame->present_event.ProcessId;
  dst_frame->swap_chain_address = src_frame->present_event.SwapChainAddress;
  strcpy_s(dst_frame->runtime,
           RuntimeToString(src_frame->present_event.Runtime));
  dst_frame->sync_interval = src_frame->present_event.SyncInterval;
  dst_frame->present_flags = src_frame->present_event.PresentFlags;

  if (src_frame->present_event.FinalState == PresentResult::Presented) {
    dst_frame->dropped = false;
    // If the last_displayed_qpc is zero this means there has not been
    // displayed frame. 
    if (src_frame->present_event.last_displayed_qpc > 0) {
      dst_frame->ms_between_display_change =
          QpcDeltaToMs(src_frame->present_event.ScreenTime -
                           src_frame->present_event.last_displayed_qpc,
                       GetQpcFrequency());
    }
    dst_frame->ms_until_displayed = QpcDeltaToMs(src_frame->present_event.ScreenTime -
                         src_frame->present_event.PresentStartTime,
        GetQpcFrequency());
  } else {
    dst_frame->dropped = true;
  }

  dst_frame->time_in_seconds = QpcDeltaToSeconds(
      src_frame->present_event.PresentStartTime - start_qpc, GetQpcFrequency());

  if (src_frame->present_event.PresentStopTime <
      src_frame->present_event.PresentStartTime) {
    dst_frame->ms_in_present_api = 0.;
  } else {
    dst_frame->ms_in_present_api =
        QpcDeltaToMs(src_frame->present_event.PresentStopTime -
                         src_frame->present_event.PresentStartTime,
                     GetQpcFrequency());
  }

  dst_frame->allows_tearing = src_frame->present_event.SupportsTearing;
  dst_frame->present_mode =
      TranslatePresentMode(src_frame->present_event.PresentMode);

  // If the ReadyTime is BEFORE the present event start time denote this
  // with a negative render complete
  if (src_frame->present_event.ReadyTime == 0) {
    dst_frame->ms_until_render_complete = 0.0;
  } else {
    if (src_frame->present_event.ReadyTime <
        src_frame->present_event.PresentStartTime) {
      dst_frame->ms_until_render_complete =
          -1.0 * QpcDeltaToMs(src_frame->present_event.PresentStartTime -
                                  src_frame->present_event.ReadyTime,
                              GetQpcFrequency());
    } else {
      dst_frame->ms_until_render_complete =
          QpcDeltaToMs(src_frame->present_event.ReadyTime -
                           src_frame->present_event.PresentStartTime,
                       GetQpcFrequency());
    }
  }
  // If the GPUStartTime is BEFORE the present event start time denote this
  // with a negative render start
  if (src_frame->present_event.GPUStartTime == 0) {
    dst_frame->ms_until_render_start = 0.0;
  } else {
    if (src_frame->present_event.GPUStartTime <
        src_frame->present_event.PresentStartTime) {
      dst_frame->ms_until_render_start =
          -1.0 * QpcDeltaToMs(src_frame->present_event.PresentStartTime -
                                  src_frame->present_event.GPUStartTime,
                              GetQpcFrequency());

    } else {
      dst_frame->ms_until_render_start =
          QpcDeltaToMs(src_frame->present_event.GPUStartTime -
                           src_frame->present_event.PresentStartTime,
                       GetQpcFrequency());
    }
  }
  dst_frame->ms_gpu_active =
      QpcDeltaToMs(src_frame->present_event.GPUDuration, GetQpcFrequency());
  dst_frame->ms_gpu_video_active = QpcDeltaToMs(
      src_frame->present_event.GPUVideoDuration, GetQpcFrequency());
  dst_frame->ms_since_input = 0.;
  if (src_frame->present_event.InputTime != 0) {
    dst_frame->ms_since_input =
        QpcDeltaToMs(src_frame->present_event.PresentStartTime -
                         src_frame->present_event.InputTime,
                     GetQpcFrequency());
  }

  // power telemetry
  dst_frame->gpu_power_w.data = src_frame->power_telemetry.gpu_power_w;
  dst_frame->gpu_power_w.valid = gpu_telemetry_cap_bits[static_cast<size_t>(
      GpuTelemetryCapBits::gpu_power)];

  dst_frame->gpu_sustained_power_limit_w.data =
      src_frame->power_telemetry.gpu_sustained_power_limit_w;
  dst_frame->gpu_sustained_power_limit_w.valid =
      gpu_telemetry_cap_bits[static_cast<size_t>(
          GpuTelemetryCapBits::gpu_sustained_power_limit)];

  dst_frame->gpu_voltage_v.data =
      src_frame->power_telemetry.gpu_voltage_v;
  dst_frame->gpu_voltage_v.valid = gpu_telemetry_cap_bits[static_cast<size_t>(
      GpuTelemetryCapBits::gpu_voltage)];

  dst_frame->gpu_frequency_mhz.data =
      src_frame->power_telemetry.gpu_frequency_mhz;
  dst_frame->gpu_frequency_mhz.valid =
      gpu_telemetry_cap_bits[static_cast<size_t>(
          GpuTelemetryCapBits::gpu_frequency)];

  dst_frame->gpu_temperature_c.data =
      src_frame->power_telemetry.gpu_temperature_c;
  dst_frame->gpu_temperature_c.valid =
      gpu_telemetry_cap_bits[static_cast<size_t>(
          GpuTelemetryCapBits::gpu_temperature)];

  dst_frame->gpu_utilization.data =
      src_frame->power_telemetry.gpu_utilization;
  dst_frame->gpu_utilization.valid =
      gpu_telemetry_cap_bits[static_cast<size_t>(
          GpuTelemetryCapBits::gpu_utilization)];

  dst_frame->gpu_render_compute_utilization.data =
      src_frame->power_telemetry.gpu_render_compute_utilization;
  dst_frame->gpu_render_compute_utilization.valid =
      gpu_telemetry_cap_bits[static_cast<size_t>(
      GpuTelemetryCapBits::gpu_render_compute_utilization)];

  dst_frame->gpu_media_utilization.data =
      src_frame->power_telemetry.gpu_media_utilization;
  dst_frame->gpu_media_utilization.valid =
      gpu_telemetry_cap_bits[static_cast<size_t>(
          GpuTelemetryCapBits::gpu_media_utilization)];

  dst_frame->vram_power_w.data = src_frame->power_telemetry.vram_power_w;
  dst_frame->vram_power_w.valid =
      gpu_telemetry_cap_bits[static_cast<size_t>(
          GpuTelemetryCapBits::vram_power)];

  dst_frame->vram_voltage_v.data = src_frame->power_telemetry.vram_voltage_v;
  dst_frame->vram_voltage_v.valid = gpu_telemetry_cap_bits[static_cast<size_t>(
      GpuTelemetryCapBits::vram_voltage)];

  dst_frame->vram_frequency_mhz.data = src_frame->power_telemetry.vram_frequency_mhz;
  dst_frame->vram_frequency_mhz.valid =
      gpu_telemetry_cap_bits[static_cast<size_t>(
      GpuTelemetryCapBits::vram_frequency)];

  dst_frame->vram_effective_frequency_gbs.data =
      src_frame->power_telemetry.vram_effective_frequency_gbps;
  dst_frame->vram_effective_frequency_gbs.valid =
      gpu_telemetry_cap_bits[static_cast<size_t>(
          GpuTelemetryCapBits::vram_effective_frequency)];

  dst_frame->vram_temperature_c.data = src_frame->power_telemetry.vram_temperature_c;
  dst_frame->vram_temperature_c.valid =
      gpu_telemetry_cap_bits[static_cast<size_t>(
          GpuTelemetryCapBits::vram_temperature)];

  for (size_t i = 0; i < MAX_PM_FAN_COUNT; i++) {
    dst_frame->fan_speed_rpm[i].data = src_frame->power_telemetry.fan_speed_rpm[i];
    dst_frame->fan_speed_rpm[i].valid =
        gpu_telemetry_cap_bits[static_cast<size_t>(
            GpuTelemetryCapBits::fan_speed_0) + i];
  }

  for (size_t i = 0; i < MAX_PM_PSU_COUNT; i++) {
    dst_frame->psu_type[i].data =
        TranslatePsuType(src_frame->power_telemetry.psu[i].psu_type);
    dst_frame->psu_type[i].valid = dst_frame->psu_power[i].valid =
        gpu_telemetry_cap_bits
            [static_cast<size_t>(GpuTelemetryCapBits::psu_info_0) + i];

    dst_frame->psu_power[i].data = src_frame->power_telemetry.psu[i].psu_power;
    dst_frame->psu_power[i].valid = gpu_telemetry_cap_bits
        [static_cast<size_t>(GpuTelemetryCapBits::psu_info_0) + i];

    dst_frame->psu_voltage[i].data = src_frame->power_telemetry.psu[i].psu_voltage;
    dst_frame->psu_voltage[i].valid = gpu_telemetry_cap_bits
        [static_cast<size_t>(GpuTelemetryCapBits::psu_info_0) + i];
  }

  // Gpu memory telemetry
  dst_frame->gpu_mem_total_size_b.data =
      src_frame->power_telemetry.gpu_mem_total_size_b;
  dst_frame->gpu_mem_total_size_b.valid =
      gpu_telemetry_cap_bits[static_cast<size_t>(
          GpuTelemetryCapBits::gpu_mem_size)];

  dst_frame->gpu_mem_used_b.data =
      src_frame->power_telemetry.gpu_mem_used_b;
  dst_frame->gpu_mem_used_b.valid =
      gpu_telemetry_cap_bits[static_cast<size_t>(
          GpuTelemetryCapBits::gpu_mem_used)];

  dst_frame->gpu_mem_max_bandwidth_bps.data =
      src_frame->power_telemetry.gpu_mem_max_bandwidth_bps;
  dst_frame->gpu_mem_max_bandwidth_bps.valid =
      gpu_telemetry_cap_bits[static_cast<size_t>(
      GpuTelemetryCapBits::gpu_mem_max_bandwidth)];

  dst_frame->gpu_mem_read_bandwidth_bps.data =
      src_frame->power_telemetry.gpu_mem_read_bandwidth_bps;
  dst_frame->gpu_mem_read_bandwidth_bps.valid =
      gpu_telemetry_cap_bits[static_cast<size_t>(
          GpuTelemetryCapBits::gpu_mem_read_bandwidth)];

  dst_frame->gpu_mem_write_bandwidth_bps.data =
      src_frame->power_telemetry.gpu_mem_write_bandwidth_bps;
  dst_frame->gpu_mem_write_bandwidth_bps.valid =
      gpu_telemetry_cap_bits[static_cast<size_t>(
          GpuTelemetryCapBits::gpu_mem_write_bandwidth)];

  // Throttling flags
  dst_frame->gpu_power_limited.data = src_frame->power_telemetry.gpu_power_limited;
  dst_frame->gpu_power_limited.valid =
      gpu_telemetry_cap_bits[static_cast<size_t>(
          GpuTelemetryCapBits::gpu_power_limited)];

  dst_frame->gpu_temperature_limited.data =
      src_frame->power_telemetry.gpu_temperature_limited;
  dst_frame->gpu_temperature_limited.valid =
      gpu_telemetry_cap_bits[static_cast<size_t>(
          GpuTelemetryCapBits::gpu_temperature_limited)];

  dst_frame->gpu_current_limited.data =
      src_frame->power_telemetry.gpu_current_limited;
  dst_frame->gpu_current_limited.valid =
      gpu_telemetry_cap_bits[static_cast<size_t>(
          GpuTelemetryCapBits::gpu_current_limited)];

  dst_frame->gpu_voltage_limited.data =
      src_frame->power_telemetry.gpu_voltage_limited;
  dst_frame->gpu_voltage_limited.valid =
      gpu_telemetry_cap_bits[static_cast<size_t>(
          GpuTelemetryCapBits::gpu_voltage_limited)];

  dst_frame->gpu_utilization_limited.data =
      src_frame->power_telemetry.gpu_utilization_limited;
  dst_frame->gpu_utilization_limited.valid =
      gpu_telemetry_cap_bits[static_cast<size_t>(
          GpuTelemetryCapBits::gpu_utilization_limited)];

  dst_frame->vram_power_limited.data =
      src_frame->power_telemetry.vram_power_limited;
  dst_frame->vram_power_limited.valid =
      gpu_telemetry_cap_bits[static_cast<size_t>(
          GpuTelemetryCapBits::vram_utilization_limited)];

  dst_frame->vram_temperature_limited.data =
      src_frame->power_telemetry.vram_temperature_limited;
  dst_frame->vram_temperature_limited.valid =
      gpu_telemetry_cap_bits[static_cast<size_t>(
          GpuTelemetryCapBits::vram_temperature_limited)];

  dst_frame->vram_current_limited.data =
      src_frame->power_telemetry.vram_current_limited;
  dst_frame->vram_current_limited.valid =
      gpu_telemetry_cap_bits[static_cast<size_t>(
          GpuTelemetryCapBits::vram_current_limited)];

  dst_frame->vram_voltage_limited.data =
      src_frame->power_telemetry.vram_voltage_limited;
  dst_frame->vram_voltage_limited.valid =
      gpu_telemetry_cap_bits[static_cast<size_t>(
          GpuTelemetryCapBits::vram_voltage_limited)];

  dst_frame->vram_utilization_limited.data =
      src_frame->power_telemetry.vram_utilization_limited;
  dst_frame->vram_utilization_limited.valid =
      gpu_telemetry_cap_bits[static_cast<size_t>(
          GpuTelemetryCapBits::vram_utilization_limited)];

  // cpu telemetry - only available in INTERNAL builds
  dst_frame->cpu_utilization.data = src_frame->cpu_telemetry.cpu_utilization;
  dst_frame->cpu_utilization.valid =
      cpu_telemetry_cap_bits[static_cast<size_t>(
          CpuTelemetryCapBits::cpu_utilization)];

  dst_frame->cpu_power_w.data = src_frame->cpu_telemetry.cpu_power_w;
  dst_frame->cpu_power_w.valid = cpu_telemetry_cap_bits[static_cast<size_t>(
      CpuTelemetryCapBits::cpu_power)];

  dst_frame->cpu_power_limit_w.data = src_frame->cpu_telemetry.cpu_power_limit_w;
  dst_frame->cpu_power_limit_w.valid =
      cpu_telemetry_cap_bits[static_cast<size_t>(
      CpuTelemetryCapBits::cpu_power_limit)];

  dst_frame->cpu_temperature_c.data = src_frame->cpu_telemetry.cpu_temperature;
  dst_frame->cpu_temperature_c.valid =
      cpu_telemetry_cap_bits[static_cast<size_t>(
          CpuTelemetryCapBits::cpu_temperature)];

  dst_frame->cpu_frequency.data = src_frame->cpu_telemetry.cpu_frequency;
  dst_frame->cpu_frequency.valid =
      cpu_telemetry_cap_bits[static_cast<size_t>(
          CpuTelemetryCapBits::cpu_frequency)];
}

// Dequeue frames from head of the named shared memory. Pop the data and 
// decrement the head_idx after read.
PM_STATUS StreamClient::DequeueFrame(PM_FRAME_DATA** out_frame_data) {
  auto nsm_view = GetNamedSharedMemView();
  auto nsm_hdr = nsm_view->GetHeader();
  if (!nsm_hdr->process_active) {
    // Service destroyed the named shared memory.
    *out_frame_data = nullptr;
    return PM_STATUS::PM_STATUS_PROCESS_NOT_EXIST;
  }

  if ((nsm_view->GetBuffer() == nullptr) || (nsm_view->IsEmpty())) {
    return PM_STATUS::PM_STATUS_NO_DATA;
  }

   uint64_t read_offset = nsm_hdr->head_idx * sizeof(PmNsmFrameData) + nsm_view->GetBaseOffset();

   PmNsmFrameData* data = reinterpret_cast<PmNsmFrameData*>(
       static_cast<char*>((nsm_view->GetBuffer())) + read_offset);

  CopyFrameData(nsm_hdr->start_qpc, data, nsm_hdr->gpuTelemetryCapBits,
                 nsm_hdr->cpuTelemetryCapBits, *out_frame_data);
  nsm_view->DequeueFrameData();
  return PM_STATUS::PM_STATUS_SUCCESS;
}

uint64_t StreamClient::GetLatestFrameIndex() {
  if (shared_mem_view_->IsEmpty()) {
    return UINT_MAX;
  }

  auto p_header = shared_mem_view_->GetHeader();

  if (!p_header->process_active) {
    LOG(ERROR) << "\n Process is not active. Shared mem view to be destroyed.";
    CloseSharedMemView();
    return UINT_MAX;
  }

  if (p_header->tail_idx == 0) {
    return p_header->max_entries - 1;
  } else {
    return (p_header->tail_idx - 1);
  }
}

// Calculate the number of frames written since the last dequue
uint64_t StreamClient::CheckPendingReadFrames() {
  uint64_t num_pending_read_frames = 0;

  auto p_header = shared_mem_view_->GetHeader();
  if (p_header->num_frames_written < current_dequeue_frame_num_) {
    // Wrap case where p_header->num_frames_written has wrapped to zero
    num_pending_read_frames = ULLONG_MAX - current_dequeue_frame_num_;
    num_pending_read_frames += p_header->num_frames_written;
  } else {
    num_pending_read_frames =
        p_header->num_frames_written - current_dequeue_frame_num_;
  }

  return num_pending_read_frames;
}

std::optional<
    std::bitset<static_cast<size_t>(GpuTelemetryCapBits::gpu_telemetry_count)>>
StreamClient::GetGpuTelemetryCaps() {
  if (shared_mem_view_->IsEmpty()) {
    return {};
  }

  auto p_header = shared_mem_view_->GetHeader();
  if (!p_header->process_active) {
    return {};
  }

  return p_header->gpuTelemetryCapBits;
}

std::optional<
    std::bitset<static_cast<size_t>(CpuTelemetryCapBits::cpu_telemetry_count)>>
StreamClient::GetCpuTelemetryCaps() {
  if (shared_mem_view_->IsEmpty()) {
    return {};
  }

  auto p_header = shared_mem_view_->GetHeader();
  if (!p_header->process_active) {
    return {};
  }

  return p_header->cpuTelemetryCapBits;
}
