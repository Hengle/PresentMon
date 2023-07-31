// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include "../PresentMonUtils/PresentMonNamedPipe.h"
#include "../PresentMonUtils/MemBuffer.h"
#include "../PresentMonUtils/NamedPipeHelper.h"
#include "../Streamer/StreamClient.h"
#include "gtest/gtest.h"

double QpcDeltaToMs(uint64_t qpc_delta, LARGE_INTEGER qpc_frequency);

class PresentMonClient {
 public:
  PresentMonClient();
  ~PresentMonClient();
  PresentMonClient(const PresentMonClient& t) = delete;
  PresentMonClient& operator=(const PresentMonClient& t) = delete;

  PM_STATUS GetFramesPerSecondData(uint32_t process_id, PM_FPS_DATA* fps_data,
                                   double window_size_in_ms,
                                   uint32_t* num_swapchains);
  PM_STATUS GetGfxLatencyData(uint32_t process_id,
                              PM_GFX_LATENCY_DATA* gfx_latency_data,
                              double window_size_in_ms,
                              uint32_t* num_swapchains);
  PM_STATUS GetGpuData(uint32_t process_id, PM_GPU_DATA* gpu_data,
                       double window_size_in_ms);
  PM_STATUS GetCpuData(uint32_t process_id, PM_CPU_DATA* cpu_data,
                       double window_size_in_ms);
  PM_STATUS SetMetricsOffset(double offset_in_ms);

  // Client API to get raw frame data and copy them into out_buf.
  //out_buf is provided by client which can hold max_frames 
  PM_STATUS GetFrameData(uint32_t process_id, bool is_etl,
                         uint32_t* in_out_num_frames,
                         PM_FRAME_DATA* out_buf);
  PM_STATUS RequestStreamProcess(uint32_t process_id);
  PM_STATUS RequestStreamProcess(char const* etl_file_name);
  PM_STATUS StopStreamProcess(uint32_t process_id);

  PM_STATUS SendRequest(MemBuffer* rqst_buf);
  PM_STATUS ReadResponse(MemBuffer* rsp_buf);
  PM_STATUS CallPmService(MemBuffer* rqst_buf, MemBuffer* rsp_buf);

  PM_STATUS EnumerateAdapters(PM_ADAPTER_INFO* adapter_info_buffer,
                              uint32_t* adapter_count);
  PM_STATUS SetActiveAdapter(uint32_t adapter_id);
  PM_STATUS GetCpuName(char* cpu_ame_buffer, uint32_t* buffer_size);
  PM_STATUS SetGPUTelemetryPeriod(uint32_t period_ms);

 private:
  FRIEND_TEST(PresentMonApiULT, TestFpsMetrics);
  FRIEND_TEST(PresentMonApiULT, TestGfxLatency);
  FRIEND_TEST(PresentMonApiULT, TestGpuMetrics);
  FRIEND_TEST(PresentMonApiULT, TestCpuMetrics);
  FRIEND_TEST(PresentMonApiULT, TestSingleEntryFpsPercentiles);
  FRIEND_TEST(PresentMonApiULT, TestSingleEntryGfxLatencyPercentiles);
  FRIEND_TEST(PresentMonApiULT, TestFpsMetricsMultiSwapChains);
  FRIEND_TEST(PresentMonApiULT, TestGfxLatencyMultiSwapChains);
  FRIEND_TEST(PresentMonApiULT, TestFpsDroppedFrames);
  FRIEND_TEST(PresentMonApiULT, TestGfxLatencyDroppedFrames);
  FRIEND_TEST(PresentMonApiULT, TestFrameCapture);
  FRIEND_TEST(PresentMonApiULT, TestTelemetryCapBits);
  FRIEND_TEST(PresentMonApiULT, TestFrameCaptureCapBits);

  struct MetricCaches {
    MetricCaches() {
      cached_fps_data_.resize(1);
      cached_latency_data_.resize(1);
      cached_gpu_data_.resize(1);
      cached_cpu_data_.resize(1);
    }
    std::vector<PM_FPS_DATA> cached_fps_data_;
    std::vector<PM_GFX_LATENCY_DATA> cached_latency_data_;
    std::vector<PM_GPU_DATA> cached_gpu_data_;
    std::vector<PM_CPU_DATA> cached_cpu_data_;
  };

  double GetPercentile(std::vector<double>& metricData, double percentile);
  
  // Helper function to calculate the 99th, 95th and 90th percentiles,
  // as well as the average of the incoming vector of metric data and
  // return the values in the passed in PM_METRIC_DOUBLE_DATA reference
  void CalculateMetricDoubleData(std::vector<double>& in_data,
                                 PM_METRIC_DOUBLE_DATA& metric_double_data,
                                 bool valid=true);
  static PM_STATUS TranslateGetLastErrorToPmStatus(DWORD last_error) {
    return PM_STATUS::PM_STATUS_ERROR;
  }
  PmNsmFrameData* GetFrameDataStart(StreamClient* client,
      uint64_t& index,
                                    double& window_sample_size_in_ms);
  bool DecrementIndex(NamedSharedMem* nsm_view, uint64_t& index);
  uint64_t GetAdjustedQpc(uint64_t current_qpc, uint64_t frame_data_qpc,
                          LARGE_INTEGER frequency);

  uint64_t ContvertMsToQpcTicks(double time_in_ms, uint64_t frequency);

  template <typename T>
  bool CopyCacheData(T* dest, uint32_t* dest_size, std::vector<T>& source) {
    bool cache_copied = false;
    // Initialize destination data
    ZeroMemory(dest, sizeof(T) * (*dest_size));
    for (uint32_t i = 0; i < (uint32_t)source.size(); i++) {
      if (i < *dest_size) {
        dest[i] = source[i];
        cache_copied = true;
      } else {
        break;
      }
    }
    *dest_size = (uint32_t)source.size();
    return cache_copied;
  }

  bool SetupClientCaches(uint32_t process_id) {
    try {
      client_metric_caches_.emplace(process_id, MetricCaches());
    } catch (...) {
      return false;
    }
    return true;
  }

  void RemoveClientCaches(uint32_t process_id) {
    auto iter = client_metric_caches_.find(process_id);
    if (iter != client_metric_caches_.end()) {
      client_metric_caches_.erase(std::move(iter));
    }
  }

  void CopyPowerTelemetryItem(
      std::vector<double>& telemetry_item, std::vector<double> psu_voltage[],
      PM_GPU_DATA* gpu_data, PresentMonPowerTelemetryInfo& power_telemetry_info,
      size_t telemetry_item_bit);
  void CopyCpuTelemetryItem(std::vector<double>& telemetry_item,
                            CpuTelemetryInfo& cpu_telemetry_info,
                            size_t telemetry_item_bit);

  void SetGpuData(std::vector<double>& telemetry_item,
                  std::vector<double> psu_voltage[], PM_GPU_DATA* gpu_data,
                  size_t telemetry_item_bit, bool valid);
  void SetCpuData(std::vector<double>& telemetry_item, PM_CPU_DATA* gpu_data,
                  size_t telemetry_item_bit, bool valid);

  void InitializeClientLogging();

  HANDLE pipe_;
  // stream clients mapping to process(id)
  std::map<uint32_t, std::unique_ptr<StreamClient>> clients_;
  // single stream etl client
  std::unique_ptr<StreamClient> etl_client_;

  uint64_t set_metric_offset_in_qpc_ticks_;
  uint64_t client_to_frame_data_delta_;

  std::unordered_map<uint32_t, MetricCaches> client_metric_caches_;

  uint32_t client_process_id_;
  bool enable_file_logging_ = false;
};

struct fps_swap_chain_data {
  fps_swap_chain_data()
      : cpu_n_time(0),
        cpu_0_time(0),
        display_n_screen_time(0),
        display_0_screen_time(0),
        render_latency_sum(0),
        display_latency_sum(0),
        display_count(0),
        num_presents(0),
        sync_interval(0),
        present_mode(PM_PRESENT_MODE_UNKNOWN),
        allows_tearing(0) {}
  std::vector<double> presented_fps;
  std::vector<double> displayed_fps;
  std::vector<double> render_latency_ms;
  std::vector<double> display_latency_ms;
  std::vector<double> frame_times_ms;
  std::vector<double> gpu_sum_ms;
  std::vector<double> dropped;
  uint64_t cpu_n_time;
  uint64_t cpu_0_time;
  uint64_t display_n_screen_time;
  uint64_t display_0_screen_time;
  uint64_t render_latency_sum = 0;
  uint64_t display_latency_sum = 0;
  uint32_t display_count;
  uint32_t num_presents;
  int32_t sync_interval;
  PM_PRESENT_MODE present_mode;
  int32_t allows_tearing;
};

void InitializeLogging(const char* location, const char* basename, const char* extension, int level);
