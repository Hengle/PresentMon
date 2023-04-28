// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

struct CpuTelemetryInfo {
  uint64_t qpc;

  double cpu_utilization;
  double cpu_power_w;
  double cpu_power_limit_w;
  double cpu_temperature;
  double cpu_frequency;

  // Internal Telemetry Only. Do not release externally.
  double cpu_bias_weight;
  double gpu_bias_weight;
};