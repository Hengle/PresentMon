// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <Windows.h>
#include <tchar.h>
#include <string>

#include <thread>
#include <vector>
#include <atomic>

const int MaxBufferLength = MAX_PATH;
const int NumReportEventStrings = 2;

class Service {
 public:
  Service(const TCHAR* serviceName);

  VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv);

  SIZE_T GetNumArguments() { return mArgv.size(); }
  std::vector<std::wstring> GetArguments() {
    return mArgv;
  };
  HANDLE GetServiceStopHandle() { return mServiceStopEventHandle; }
  HANDLE GetResetPowerTelemetryHandle() {
    return mResetPowerTelemetryEventHandle;
  }
  VOID ReportServiceStatus(DWORD currentState, DWORD win32ExitCode,
                                  DWORD waitHint);
 private:
  VOID ServiceInit(DWORD argc, LPTSTR* argv);

  SERVICE_STATUS mServiceStatus{};
  SERVICE_STATUS_HANDLE mServiceStatusHandle{};
  HANDLE mEventLogHandle = nullptr;
  std::wstring mServiceName;
  HANDLE mServiceStopEventHandle = nullptr;
  HANDLE mResetPowerTelemetryEventHandle = nullptr;
  HDEVNOTIFY mDeviceNotifyHandle = nullptr;
  DWORD mCheckPoint = 1;
  std::atomic<bool> mResetProviders = true;

  // Incoming arguments from Service Main.
  std::vector<std::wstring> mArgv;
};

DWORD WINAPI PresentMonMainThread(LPVOID lpParam);