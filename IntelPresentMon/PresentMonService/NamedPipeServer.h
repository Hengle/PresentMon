// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <thread>
#include <vector>

#include "..\PresentMonUtils\MemBuffer.h"
#include "PresentMon.h"
#include "..\PresentMonUtils\PresentMonNamedPipe.h"
#include "Service.h"
#include "sddl.h"

const size_t MaxBufferSize = 4096;

class NamedPipeServer {
 public:
  NamedPipeServer(Service* srv, PresentMon* pm);
  ~NamedPipeServer();
  NamedPipeServer(const NamedPipeServer& t) = delete;
  NamedPipeServer& operator=(const NamedPipeServer& t) = delete;

  DWORD RunServer();

 private:
  enum class PipeStates { CONNECTING, READING, WRITING };

  struct PipeInstanceDeleter {
    void operator()(HANDLE pipeInstance) const {
      if (pipeInstance != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(pipeInstance);
        DisconnectNamedPipe(pipeInstance);
        CloseHandle(pipeInstance);
      }
    }
  };

  class Pipe {
   public:
    Pipe()
        : mRequestBuffer(std::make_unique<MemBuffer>()),
          mResponseBuffer(std::make_unique<MemBuffer>()) {}
    Pipe(const Pipe& t) = delete;
    Pipe& operator=(const Pipe& t) = delete;

    DWORD CreatePipeInstance(LPCTSTR pipe_name, int max_pipes, uint32_t pipe_timeout) {
      SECURITY_ATTRIBUTES sa = {sizeof(sa)};
      if (ConvertStringSecurityDescriptorToSecurityDescriptorW(
              L"D:PNO_ACCESS_CONTROLS:(ML;;NW;;;LW)", SDDL_REVISION_1,
              &sa.lpSecurityDescriptor, NULL)) {
        HANDLE tempPipeInstance =
            CreateNamedPipe(pipe_name,                   // pipe name
                            PIPE_ACCESS_DUPLEX |         // read/write access
                                FILE_FLAG_OVERLAPPED,    // overlapped mode
                            PIPE_TYPE_MESSAGE |          // message-type pipe
                                PIPE_READMODE_MESSAGE |  // message-read mode
                                PIPE_WAIT,               // blocking mode
                            max_pipes,                   // number of instances
                            MaxBufferSize,               // output buffer size
                            MaxBufferSize,               // input buffer size
                            pipe_timeout,                // client time-out
                            &sa);  // default security attributes
        if (tempPipeInstance == INVALID_HANDLE_VALUE) {
          return GetLastError();
        }
        mPipeInstance.reset(tempPipeInstance);
        LocalFree(sa.lpSecurityDescriptor);
        return ERROR_SUCCESS;
      } else {
        return GetLastError();
      }
    }

    BYTE mPipeReadBuffer[MaxBufferSize];
    IPMSMResponseHeader mServerErrorResponse = {};
    std::unique_ptr<MemBuffer> mRequestBuffer;
    std::unique_ptr<MemBuffer> mResponseBuffer;
    OVERLAPPED mOverlap = {};
    std::unique_ptr<std::remove_pointer_t<HANDLE>, PipeInstanceDeleter>
        mPipeInstance;
    DWORD mBytesRead = 0;
    PipeStates mCurrentState = PipeStates::CONNECTING;
    BOOL mPendingIO = FALSE;
  };

  DWORD CreateNamedPipesAndEvents();
  DWORD ConnectToNewClient(HANDLE pipe, LPOVERLAPPED overlapped,
                           BOOL& pendingIO);
  void GetPendingOperationResult(DWORD pipeIndex);
  void DisconnectAndReconnect(DWORD pipeIndex);
  void ExecuteCurrentState(DWORD pipeIndex);
  void ReadRequestMessage(DWORD pipeIndex);
  void WriteReplyMessage(DWORD pipeIndex);
  void EvaluateAndRespondToRequestMessage(DWORD pipeIndex);
  DWORD GetNumActiveConnections();

  LPCTSTR mPresentMonServicePipeName =
      TEXT("\\\\.\\pipe\\presentmonsvcnamedpipe");
  static const int mMaxPipes = 5;
  static const int mMaxEvents = mMaxPipes + 1;
  const UINT32 mPipeTimeout = 5000;

  Service* mService;
  PresentMon* mPm;
  Pipe mPipe[mMaxPipes];
  HANDLE mEvents[mMaxEvents];
};