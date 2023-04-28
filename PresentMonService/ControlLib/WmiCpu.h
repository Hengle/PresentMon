// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#define NOMINMAX
#include <Windows.h>
#include <pdh.h>
#include "CpuTelemetry.h"
#include "TelemetryHistory.h"
#include <mutex>
#include <optional>

namespace pwr::cpu::wmi {

class WmiCpu : public CpuTelemetry {
 public:
  WmiCpu();
  ~WmiCpu();
  bool Sample() noexcept override;
  std::optional<CpuTelemetryInfo> GetClosest(
      uint64_t qpc) const noexcept override;

  // types
  class NonGraphicsDeviceException : public std::exception {};

 private:

  // data
  HQUERY query_ = nullptr;
  HCOUNTER processor_frequency_counter_;
  HCOUNTER processor_performance_counter_;
  HCOUNTER processor_time_counter_;
  LARGE_INTEGER next_sample_qpc_ = {};
  LARGE_INTEGER frequency_ = {};

  mutable std::mutex history_mutex_;
  TelemetryHistory<CpuTelemetryInfo> history_{CpuTelemetry::defaultHistorySize};
};

}  // namespace pwr::cpu::wmi