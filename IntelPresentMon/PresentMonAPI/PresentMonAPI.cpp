// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
// PresentMonAPI.cpp : Defines the functions for the static library.
#include "PresentMonAPI.h"

#include <Windows.h>

#include <algorithm>
#include <unordered_map>

#include "../PresentMonClient/PresentMonClient.h"

// Global pointer to PresentMon Client object
static PresentMonClient* gPmClient = nullptr;

PM_STATUS pmInitialize() {
  if (gPmClient == nullptr) {
    try {
      gPmClient = new PresentMonClient();
    } catch (...) {
      return PM_STATUS::PM_STATUS_SERVICE_ERROR;
    }
  }
  return PM_STATUS::PM_STATUS_SUCCESS;
}

PM_STATUS pmShutdown() {
  if (gPmClient) {
    delete gPmClient;
    gPmClient = nullptr;
  }

  return PM_STATUS::PM_STATUS_SUCCESS;
}

PM_STATUS pmGetFramesPerSecondData(uint32_t processId, PM_FPS_DATA* pmFpsData,
                                   double windowSizeinMs,
                                   uint32_t* numSwapChains) {
  if (gPmClient) {
    return gPmClient->GetFramesPerSecondData(processId, pmFpsData,
                                             windowSizeinMs, numSwapChains);
  } else {
    return PM_STATUS::PM_STATUS_ERROR;
  }
}

PM_STATUS pmGetGfxLatencyData(uint32_t processId,
                              PM_GFX_LATENCY_DATA* pmGfxLatencyData,
                              double windowSizeinMs, uint32_t* numSwapChains) {
  if (gPmClient) {
    return gPmClient->GetGfxLatencyData(processId, pmGfxLatencyData,
                                        windowSizeinMs, numSwapChains);
  } else {
    return PM_STATUS::PM_STATUS_ERROR;
  }
}

PM_STATUS pmGetFrameData(uint32_t processId, uint32_t* in_out_num_frames,
                         PM_FRAME_DATA* out_data) {
  if (gPmClient) {
    return gPmClient->GetFrameData(processId, false, in_out_num_frames,
                                   out_data);
  } else {
    return PM_STATUS::PM_STATUS_ERROR;
  }
}

PM_STATUS pmGetStreamAllFrameData(uint32_t* in_out_num_frames,
                                  PM_FRAME_DATA* out_data) {
  if (gPmClient) {
    return gPmClient->GetFrameData(
        static_cast<uint32_t>(StreamPidOverride::kStreamAllPid), false,
        in_out_num_frames, out_data);
  } else {
    return PM_STATUS::PM_STATUS_ERROR;
  }
}

PM_STATUS pmGetEtlFrameData(uint32_t* in_out_num_frames,
                            PM_FRAME_DATA* out_data) {
  if (gPmClient) {
    return gPmClient->GetFrameData(0, true, in_out_num_frames, out_data);
  } else {
    return PM_STATUS::PM_STATUS_ERROR;
  }
}

PM_STATUS pmGetGPUData(uint32_t processId, PM_GPU_DATA* pmGpuData,
                       double windowSizeinMs) {
  if (gPmClient) {
    return gPmClient->GetGpuData(processId, pmGpuData, windowSizeinMs);
  } else {
    return PM_STATUS::PM_STATUS_ERROR;
  }
}

PM_STATUS pmGetCPUData(uint32_t processId, PM_CPU_DATA* pmCpuData,
                       double windowSizeinMs) {
  if (gPmClient) {
    return gPmClient->GetCpuData(processId, pmCpuData, windowSizeinMs);
  } else {
    return PM_STATUS::PM_STATUS_ERROR;
  }
}

PM_STATUS pmStartStream(uint32_t processId) {
  if (gPmClient == nullptr) {
    return PM_STATUS::PM_STATUS_SERVICE_NOT_INITIALIZED;
  }

  return gPmClient->RequestStreamProcess(processId);
}

PM_STATUS pmStartStreamEtl(char const* etl_file_name) {
  if (gPmClient == nullptr) {
    return PM_STATUS::PM_STATUS_SERVICE_NOT_INITIALIZED;
  }

  return gPmClient->RequestStreamProcess(etl_file_name);
}

PM_STATUS pmStopStream(uint32_t processId) {
  if (gPmClient == nullptr) {
    return PM_STATUS::PM_STATUS_SERVICE_NOT_INITIALIZED;
  }

  return gPmClient->StopStreamProcess(processId);
}

PM_STATUS pmSetMetricsOffset(double offset_in_ms) {
  if (gPmClient) {
    return gPmClient->SetMetricsOffset(offset_in_ms);
  } else {
    return PM_STATUS::PM_STATUS_ERROR;
  }
}

PRESENTMON_API_EXPORT PM_STATUS pmEnumerateAdapters(
    PM_ADAPTER_INFO* adapter_info_buffer, uint32_t* adapter_count) {
  if (!adapter_count) {
    return PM_STATUS::PM_STATUS_ERROR;
  }
  if (gPmClient) {
    return gPmClient->EnumerateAdapters(adapter_info_buffer, adapter_count);
  } else {
    return PM_STATUS::PM_STATUS_SERVICE_NOT_INITIALIZED;
  }
}

PRESENTMON_API_EXPORT PM_STATUS pmSetActiveAdapter(uint32_t adapter_id) {
  if (gPmClient) {
    return gPmClient->SetActiveAdapter(adapter_id);
  } else {
    return PM_STATUS::PM_STATUS_SERVICE_NOT_INITIALIZED;
  }
}

PRESENTMON_API_EXPORT PM_STATUS pmGetCpuName(char* cpu_name_buffer,
                                             uint32_t* buffer_size) {
  if (gPmClient) {
    return gPmClient->GetCpuName(cpu_name_buffer, buffer_size);
  } else {
    return PM_STATUS::PM_STATUS_SERVICE_NOT_INITIALIZED;
  }
}

PRESENTMON_API_EXPORT PM_STATUS pmSetGPUTelemetryPeriod(uint32_t period_ms) {
  if (gPmClient) {
    if (period_ms >= MIN_PM_TELEMETRY_PERIOD &&
        period_ms <= MAX_PM_TELEMETRY_PERIOD) {
      return gPmClient->SetGPUTelemetryPeriod(period_ms);
    } else {
      return PM_STATUS::PM_STATUS_OUT_OF_RANGE;
    }
  } else {
    return PM_STATUS::PM_STATUS_SERVICE_NOT_INITIALIZED;
  }
}

PRESENTMON_API_EXPORT void pmInitializeLogging(const char* location, const char* basename, const char* extension, int level)
{
    InitializeLogging(location, basename, extension, level);
}