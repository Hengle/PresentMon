// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include "CpuTelemetryInfo.h"

namespace pwr::cpu
{
class CpuTelemetry {
 public:
  virtual ~CpuTelemetry() = default;
  virtual bool Sample() noexcept = 0;
  virtual std::optional<CpuTelemetryInfo> GetClosest(
      uint64_t qpc) const noexcept = 0;

  // constants
  static constexpr size_t defaultHistorySize = 300;
};
}