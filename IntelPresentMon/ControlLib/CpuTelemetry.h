// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <bitset>
#include <vector>
#include <Wbemidl.h>
#include <comdef.h>
#include <wrl/client.h>
#include <stdexcept>
#include "CpuTelemetryInfo.h"
#include "..\PresentMonUtils\StringUtils.h"

namespace pwr::cpu
{
class CpuTelemetry {
 public:
  virtual ~CpuTelemetry() = default;
  virtual bool Sample() noexcept = 0;
  virtual std::optional<CpuTelemetryInfo> GetClosest(
      uint64_t qpc) const noexcept = 0;
  void SetTelemetryCapBit(CpuTelemetryCapBits telemetryCapBit) noexcept
  {
      cpuTelemetryCapBits_.set(static_cast<size_t>(telemetryCapBit));
  }
  std::bitset<static_cast<size_t>(CpuTelemetryCapBits::cpu_telemetry_count)> GetCpuTelemetryCapBits()
  {
      return cpuTelemetryCapBits_;
  }

  std::string GetCpuName() {
      if (cpu_name_.size() == 0) {
        std::wstring local_cpu_name{};
        ExecuteWQLProcessorNameQuery(local_cpu_name);
        if (local_cpu_name.size() > 0) {
          // Only return first entry for cpu name
          cpu_name_ = ConvertFromWideString(std::move(local_cpu_name));
        } else {
          cpu_name_ = "UNKNOWN_CPU";
        }
      }

      return cpu_name_;
  }

  // constants
  static constexpr size_t defaultHistorySize = 300;
  // data
 private:

  void ExecuteWQLProcessorNameQuery(std::wstring& processor_name) {
      struct ComHelper {
        ComHelper() {
          HRESULT result{CoInitializeEx(nullptr, COINIT_MULTITHREADED)};

          if (FAILED(result)) {
            throw std::runtime_error("Failed CoInitializeEx");
          }

          result = CoInitializeSecurity(
              nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT,
              RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);

          if (FAILED(result)) {
            CoUninitialize();
            throw std::runtime_error("Failed CoInitializeSecurity");
          }
        }
        ComHelper(const ComHelper& t) = delete;
        ComHelper& operator=(const ComHelper& t) = delete;
        ~ComHelper() { CoUninitialize(); }
      };

      // Initialize to empty state
      processor_name.clear();

      try {
        ComHelper com_helper;

        std::wstring query(L"SELECT Name FROM Win32_Processor");
        std::wstring server_name(L"ROOT\\CIMV2");

        Microsoft::WRL::ComPtr<IWbemLocator> locator;
        HRESULT result =
            CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
                             IID_IWbemLocator, (&locator));

        if (FAILED(result)) {
          return;
        }

        Microsoft::WRL::ComPtr<IWbemServices> services;
        result =
            locator->ConnectServer(_bstr_t(server_name.c_str()), nullptr, nullptr,
                                   nullptr, NULL, nullptr, nullptr, &services);

        if (FAILED(result)) {
          return;
        }

        result =
            CoSetProxyBlanket(services.Get(), RPC_C_AUTHN_WINNT,
                              RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL,
                              RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

        if (FAILED(result)) {
          return;
        }

        Microsoft::WRL::ComPtr<IEnumWbemClassObject> enumerator;
        result = services->ExecQuery(
            bstr_t(L"WQL"), bstr_t(query.c_str()),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
            &enumerator);

        if (FAILED(result)) {
          return;
        }

        IWbemClassObject* ClassObject{};
        DWORD returned{};
        result = enumerator->Next(WBEM_INFINITE, 1, &ClassObject, &returned);

        if (FAILED(result) || returned == 0) {
            return;
          }

        VARIANT variant{};
        result = ClassObject->Get(L"Name", 0, &variant, nullptr, nullptr);
        if (FAILED(result)) {
          return;
        }

        if (variant.vt == VT_BSTR) {
          processor_name = variant.bstrVal;
        }

        VariantClear(&variant);
        ClassObject->Release();
      } catch (...) {}
  }

  std::bitset<static_cast<size_t>(CpuTelemetryCapBits::cpu_telemetry_count)>
      cpuTelemetryCapBits_{};
  std::string cpu_name_;
};
}