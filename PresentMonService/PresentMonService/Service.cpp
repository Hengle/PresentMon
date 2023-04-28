// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "Service.h"

#include <assert.h>
#include <strsafe.h>

SERVICE_STATUS Service::mServiceStatus;
SERVICE_STATUS_HANDLE Service::mServiceStatusHandle;
HANDLE Service::mServiceStopEventHandle;
DWORD Service::mCheckPoint;

Service::Service(const TCHAR* serviceName) : mEventLogHandle(nullptr) {
  ZeroMemory(&mServiceStatus, sizeof(mServiceStatus));

  mServiceStatusHandle = nullptr;
  mServiceStopEventHandle = nullptr;
  mCheckPoint = 1;
  mArgc = 0;

  mEventLogHandle = RegisterEventSource(NULL, serviceName);

  // Save the incoming service name for later reporting
  assert((_tcslen(serviceName) + 1) < MaxBufferLength);
  _tcscpy_s(mServiceName, MaxBufferLength, serviceName);
}

Service::~Service() {
  if (mEventLogHandle) {
    DeregisterEventSource(mEventLogHandle);
    mEventLogHandle = nullptr;
  }

  if (mServiceStopEventHandle) {
    CloseHandle(mServiceStopEventHandle);
    mServiceStopEventHandle = nullptr;
  }

  for (int i = 0; i < mArgv.size(); i++) {
    TCHAR* tempArgv = mArgv[i];
    delete[] tempArgv;
  }
  std::vector<LPTSTR>().swap(mArgv);
}

DWORD Service::GetNumArguments() { return mArgc; }

std::vector<LPTSTR> Service::GetArguments() { return mArgv; }

VOID WINAPI Service::ServiceMain(DWORD argc, LPTSTR* argv,
                                 const TCHAR* serviceName) {
  mServiceStatusHandle =
      RegisterServiceCtrlHandler(serviceName, ServiceCtrlHandler);

  if (mServiceStatusHandle == nullptr) {
    ServiceReportEvent(TEXT("RegisterServiceCtrlHandler"));
    return;
  }

  mServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  mServiceStatus.dwServiceSpecificExitCode = 0;

  // TODO(megalvan): Define the correct pending time instead of 3000
  ReportServiceStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

  ServiceInit(argc, argv);

  return;
}

VOID Service::ServiceInit(DWORD argc, LPTSTR* argv) {
  // Create an event. The control handler function, ServiceCtrlHandler,
  // signals this event when it receives the stop control code from the
  // SCM.
  mServiceStopEventHandle = CreateEvent(NULL,   // default security attributes
                                        TRUE,   // manual reset event
                                        FALSE,  // not signaled
                                        NULL);  // no name

  if (mServiceStopEventHandle == nullptr) {
    ReportServiceStatus(SERVICE_STOPPED, GetLastError(), 0);
    return;
  }

  // Save off the passed in arguments for later use by PresentMon
  mArgc = argc;
  for (DWORD i = 0; i < mArgc; i++) {
    TCHAR* tempArgv = new TCHAR[_tcslen(argv[i]) + 1];
    if (tempArgv == nullptr) {
      break;
    }
    _tcscpy_s(tempArgv, _tcslen(argv[i]) + 1, argv[i]);
    mArgv.push_back(tempArgv);
  }

  // Start the main PresentMon thread
  std::thread pmMainThread(PresentMonMainThread, this);

  ReportServiceStatus(SERVICE_RUNNING, NO_ERROR, 0);

  // Check if we should stop our service
  WaitForSingleObject(mServiceStopEventHandle, INFINITE);

  if (pmMainThread.joinable()) {
    pmMainThread.join();
  }

  ReportServiceStatus(SERVICE_STOPPED, NO_ERROR, 0);
  return;
}

VOID Service::ReportServiceStatus(DWORD currentState, DWORD win32ExitCode,
                                  DWORD waitHint) {
  mServiceStatus.dwCurrentState = currentState;
  mServiceStatus.dwWin32ExitCode = win32ExitCode;
  mServiceStatus.dwWaitHint = waitHint;

  if (currentState == SERVICE_START_PENDING) {
    // If we are in a PENDING state do not accept any controls.
    mServiceStatus.dwControlsAccepted = 0;
    // Increment the check point to indicate to the SCM progress
    // is being made
    mServiceStatus.dwCheckPoint = mCheckPoint++;
  } else if (currentState == SERVICE_RUNNING ||
             currentState == SERVICE_STOP_PENDING ||
             currentState == SERVICE_STOPPED) {
    // If we transitioned to a RUNNING, STOP_PENDING or STOPPED state
    // we accept a STOP control
    mServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    mServiceStatus.dwCheckPoint = 0;
  } else {
    // We only set START_PENDING, RUNNING, STOP_PENDING and STOPPED.
    // Please check this incoming currentState.
    assert(false);
  }

  SetServiceStatus(mServiceStatusHandle, &mServiceStatus);
}

VOID Service::ServiceReportEvent(const TCHAR* functionName) {
  TCHAR buffer[MaxBufferLength];
  LPCTSTR strings[NumReportEventStrings];

  if (mEventLogHandle) {
    StringCchPrintf(buffer, MaxBufferLength, TEXT("%s failed with %d"),
                    functionName, GetLastError());

    strings[0] = mServiceName;
    strings[1] = buffer;

    // TODO(megalvan): Enable
    /*
    ReportEvent(
        mEventLogHandle,
        EVENTLOG_ERROR_TYPE,
        0,
        SVC_ERROR,
        NULL,
        NumReportEventStrings,
        0,
        strings,
        NULL);
    */
  }
}

HANDLE Service::GetServiceStopHandle() { return mServiceStopEventHandle; }

VOID WINAPI Service::ServiceCtrlHandler(DWORD control) {
  switch (control) {
    case SERVICE_CONTROL_STOP:
      ReportServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

      // Set the event to shut down our service
      SetEvent(mServiceStopEventHandle);
      break;
    case SERVICE_CONTROL_INTERROGATE:
      break;
    default:
      break;
  }
}
