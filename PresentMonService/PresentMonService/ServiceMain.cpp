// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include <Windows.h>
#include <tchar.h>

#include "Service.h"

TCHAR serviceName[MaxBufferLength] = TEXT("Intel PresentMon Service");
Service* gService;

VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv);

int __cdecl _tmain(int argc, TCHAR* argv[]) {
  gService = new Service(serviceName);
  if (gService == nullptr) {
    return -1;
  }

  SERVICE_TABLE_ENTRY dispatchTable[] = {
      {serviceName, static_cast<LPSERVICE_MAIN_FUNCTION>(ServiceMain)},
      {NULL, NULL}};

  if (!StartServiceCtrlDispatcher(dispatchTable)) {
    gService->ServiceReportEvent(TEXT("StartServiceCtrlDispatcher"));
  }

  delete gService;
  return 0;
}

VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv) {
  gService->ServiceMain(argc, argv, serviceName);
}