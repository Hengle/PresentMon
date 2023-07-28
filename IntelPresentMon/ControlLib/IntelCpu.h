// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#define NOMINMAX
#include <Windows.h>
#include "ipf_core_api.h"
#include "CpuTelemetry.h"
#include "TelemetryHistory.h"
#include <mutex>
#include <optional>

namespace pwr::cpu::intel {
class IntelCpu : public CpuTelemetry {
 public:
  IntelCpu();
  IntelCpu(const IntelCpu& t) = delete;
  IntelCpu& operator=(const IntelCpu& t) = delete;
  ~IntelCpu();
  bool Sample() noexcept override;
  std::optional<CpuTelemetryInfo> GetClosest(
      uint64_t qpc) const noexcept override;

  // types
  class NonGraphicsDeviceException : public std::exception {};

 private:
  // functions
  bool CreateIpfSession();
  bool SubmitIpfExecuteCommand(EsifData* command, EsifData* request,
                               EsifData* response);
  // data
  IpfSession_t ipf_session_ = IPF_INVALID_SESSION;
  std::string cpu_name_;

  mutable std::mutex history_mutex_;
  TelemetryHistory<CpuTelemetryInfo> history_{CpuTelemetry::defaultHistorySize};
};
}  // namespace pwr::cpu::intel