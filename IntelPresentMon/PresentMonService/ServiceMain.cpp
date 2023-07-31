// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include <Windows.h>
#include <tchar.h>

#include "Service.h"

TCHAR serviceName[MaxBufferLength] = TEXT("Intel PresentMon Service");

VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv);

int __cdecl _tmain(int argc, TCHAR* argv[]) {

  SERVICE_TABLE_ENTRY dispatchTable[] = {
      {serviceName, static_cast<LPSERVICE_MAIN_FUNCTION>(ServiceMain)},
      {NULL, NULL}};

  if (!StartServiceCtrlDispatcher(dispatchTable)) {
    return 1;
  }

  return 0;
}

VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv) {
  Service present_mon_service(serviceName);

  present_mon_service.ServiceMain(argc, argv);
}