// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <Windows.h>
#include <tchar.h>

#include <thread>
#include <vector>

const int MaxBufferLength = MAX_PATH;
const int NumReportEventStrings = 2;

class Service {
 public:
  Service(const TCHAR* serviceName);
  ~Service();

  VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv, const TCHAR* serviceName);
  static VOID WINAPI ServiceCtrlHandler(DWORD);
  VOID ServiceReportEvent(const TCHAR* szFunction);

  DWORD GetNumArguments();
  std::vector<LPTSTR> GetArguments();
  HANDLE GetServiceStopHandle();

 private:
  static VOID ReportServiceStatus(DWORD currentState, DWORD win32ExitCode,
                                  DWORD waitHint);
  VOID ServiceInit(DWORD argc, LPTSTR* argv);

  static SERVICE_STATUS mServiceStatus;
  static SERVICE_STATUS_HANDLE mServiceStatusHandle;
  HANDLE mEventLogHandle;
  TCHAR mServiceName[MaxBufferLength];
  static HANDLE mServiceStopEventHandle;

  static DWORD mCheckPoint;

  // Incoming arguments from Service Main. Used by
  // PresentMon main thread
  DWORD mArgc;
  std::vector<LPTSTR> mArgv;
};

DWORD WINAPI PresentMonMainThread(LPVOID lpParam);