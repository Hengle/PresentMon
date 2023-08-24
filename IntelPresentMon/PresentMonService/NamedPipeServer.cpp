// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "NamedPipeServer.h"
#include <Windows.h>
#include <strsafe.h>
#include <tchar.h>
#include <thread>
#include "NamedPipeCmdProcess.h"
#include "..\PresentMonUtils\NamedPipeHelper.h"
#include "sddl.h"

NamedPipeServer::NamedPipeServer(Service* srv, PresentMon* pm)
    : mService(srv), mPm(pm) {
  // Initialize all events associated with a pipe
  for (DWORD i = 0; i < mMaxPipes; i++) {
    mEvents[i] = INVALID_HANDLE_VALUE;
  }

  // Initialize the final element of the event
  // array using the stop handle from the passed service.
  mEvents[mMaxPipes] = srv->GetServiceStopHandle();
}

NamedPipeServer::~NamedPipeServer() {
  // Flush all pipes to allow the client to read the pipes contents
  // before disconnecting. Then disconnect the pipes, and close the
  // handle to the pipe instances.
  for (DWORD i = 0; i < mMaxPipes; i++) {
    // Close all of the events associated with the pipes. The passed
    // in stop event must be closed by the calling function.
    CloseHandle(mEvents[i]);
  }
}

DWORD NamedPipeServer::CreateNamedPipesAndEvents() {
  for (DWORD i = 0; i < mMaxPipes; i++) {
    // Create an event object for this instance.

    mEvents[i] = CreateEvent(NULL,   // default security attribute
                             TRUE,   // manual-reset event
                             TRUE,   // initial state = signaled
                             NULL);  // unnamed event object

    if (mEvents[i] == NULL) {
      return GetLastError();
    }

    // Save the event in the overlap structure
    mPipe[i].mOverlap.hEvent = mEvents[i];

    // Create the pipe instance
    if (const auto result = mPipe[i].CreatePipeInstance(
            mPresentMonServicePipeName, mMaxPipes, mPipeTimeout);
        result != ERROR_SUCCESS) {
      return result;
    }

    // Call the subroutine to connect to the new client
    DWORD connectionStatus = ConnectToNewClient(
        mPipe[i].mPipeInstance.get(), &mPipe[i].mOverlap, mPipe[i].mPendingIO);

    if (connectionStatus == ERROR_SUCCESS) {
      // Set the current state based on the result of the
      // ConnectNamedPipe operation. If we have operation is
      // still pending set the state to connecting. If
      mPipe[i].mCurrentState = mPipe[i].mPendingIO ? PipeStates::CONNECTING
                                                   :     // still connecting
                                   PipeStates::READING;  // ready to read
    } else {
      return connectionStatus;
    }
  }

  return ERROR_SUCCESS;
}

// This function is called to start an overlapped connect operation.
DWORD NamedPipeServer::ConnectToNewClient(HANDLE pipe, LPOVERLAPPED overlapped,
                                          BOOL& pendingIO) {
  // Start an overlapped connection for this pipe instance.
  BOOL connected = ConnectNamedPipe(pipe, overlapped);

  // Initialize that we have no pending IO operation
  pendingIO = FALSE;

  // An asynchronus overlapped ConnectNamedPipe should return zero.
  if (connected == TRUE) {
    return GetLastError();
  }

  switch (GetLastError()) {
    case ERROR_IO_PENDING:
      // The overlapped connection operation is still pending. This is
      // not an error, simply set that we have pendingIO.
      pendingIO = TRUE;
      break;
    case ERROR_PIPE_CONNECTED:
      // A client has already connected. This can happen if a client
      // connects between the call to CreateNamedPipe and the call to
      // ConnectNamedPipe. Manually set the event for this pipe so
      // we can start reading right away.
      if (SetEvent(overlapped->hEvent)) {
        // If we were able to set the event break out, else fall
        // through and report the error.
        break;
      }
    default:
      // If an error occurs during the connect operation...
      return GetLastError();
  }

  return ERROR_SUCCESS;
}

DWORD NamedPipeServer::RunServer() {
  DWORD i;

  DWORD lastError = CreateNamedPipesAndEvents();
  if (lastError != ERROR_SUCCESS) {
    return lastError;
  }

  while (1) {
    // Wait for the event object to be signaled, indicating
    // completion of an overlapped read, write, connect or
    // stop operation.
    DWORD waitResult = WaitForMultipleObjects(
        mMaxEvents,  // number of event objects, including the passed in stop
                     // event
        mEvents,     // array of event objects
        FALSE,       // does not wait for all
        INFINITE);   // waits indefinitely

    // waitResult shows which pipe completed the operation.
    i = waitResult - WAIT_OBJECT_0;
    if (i > (mMaxPipes - 1)) {
      if (i == (mMaxEvents - 1)) {
        // If the returned event index is equal to the the last entry
        // then the stop event has been signaled and its time to go
        return ERROR_SUCCESS;
      }
    }

    GetPendingOperationResult(i);
    ExecuteCurrentState(i);
  }
}

// This function completes any pending operation by calling
// GetOverlappedResult and transitioning the specified pipe
// to the next state.
void NamedPipeServer::GetPendingOperationResult(DWORD pipeIndex) {
  // If an operation was pending get the result and
  // then set the current state.
  if (mPipe[pipeIndex].mPendingIO) {
    DWORD numBytesTransferred;

    // The GetOverlappedResult call retrieves the results of an
    // overlapped operation on the specified named pipe. If the
    // call is successful the data will populate the buffers
    // specified in the original read or write operation.
    BOOL success =
        GetOverlappedResult(mPipe[pipeIndex].mPipeInstance.get(),  // handle to pipe
                            &mPipe[pipeIndex].mOverlap,  // OVERLAPPED structure
                            &numBytesTransferred,        // bytes transferred
                            FALSE);                      // do not wait

    switch (mPipe[pipeIndex].mCurrentState) {
      case PipeStates::CONNECTING:
        // Pending connect operation
        if (!success) {
          return;
        }
        // We were in the connecting state and the overlapped result
        // was successful. Now transition to a reading state to read
        // the incoming data.
        mPipe[pipeIndex].mCurrentState = PipeStates::READING;
        break;
      case PipeStates::READING:
        // Pending read operation
        if (!success && numBytesTransferred == 0) {
          // We were in the reading state and the read result
          // was NOT successful and no bytes were transferred.
          // Disconnect and reconnect the specified pipe.
          DisconnectAndReconnect(pipeIndex);
          return;
        } else if (!success && numBytesTransferred > 0) {
          // We were in the reading state and the read result
          // was not successuful, however there were bytes transferred.
          // This means the incoming request data was larger than our pipe
          // and we need to make successive ReadFile calls to get all the data.
          // Add the read data to the request memory buffer.
          mPipe[pipeIndex].mRequestBuffer->AddItem(
              mPipe[pipeIndex].mPipeReadBuffer, numBytesTransferred);
        } else {
          // We were in the reading state and the overlapped result
          // was successful. Add the data from the pip and transition to the
          // writing state to process the incoming data and form a response.
          mPipe[pipeIndex].mRequestBuffer->AddItem(
              mPipe[pipeIndex].mPipeReadBuffer, numBytesTransferred);
          mPipe[pipeIndex].mBytesRead = numBytesTransferred;
          mPipe[pipeIndex].mCurrentState = PipeStates::WRITING;
        }
        break;
      case PipeStates::WRITING:
        // Pending write operation
        if (!success ||
            numBytesTransferred !=
                mPipe[pipeIndex].mResponseBuffer->GetCurrentSize()) {
          // We were in the writing state and the overlapped result
          // was NOT successful and the number of bytes transferred did not
          // match the number of bytes were supposed to transfer.
          // Disconnect and reconnect the specified pipe.
          DisconnectAndReconnect(pipeIndex);
          return;
        }
        // We were in the writing state and the overlapped result
        // was successful. Now transition to the reading state.
        mPipe[pipeIndex].mCurrentState = PipeStates::READING;
        break;
    }
  }

  return;
}

// This function is called when an error occurs or when the client
// closes its handle to the pipe. Disconnect from this client, then
// call ConnectNamedPipe to wait for another client to connect.
void NamedPipeServer::DisconnectAndReconnect(DWORD pipeIndex) {
  // Disconnect the pipe instance.
  if (!DisconnectNamedPipe(mPipe[pipeIndex].mPipeInstance.get())) {
    return;
  }

  DWORD connectionStatus = ConnectToNewClient(mPipe[pipeIndex].mPipeInstance.get(),
                                              &mPipe[pipeIndex].mOverlap,
                                              mPipe[pipeIndex].mPendingIO);

  if (connectionStatus == ERROR_SUCCESS) {
    // Set the current state based on the result of the
    // ConnectNamedPipe operation. If we have operation is
    // still pending set the state to connecting. If
    mPipe[pipeIndex].mCurrentState = mPipe[pipeIndex].mPendingIO
                                         ? PipeStates::CONNECTING
                                         :  // still connecting
                                         PipeStates::READING;  // ready to read
  }

  return;
}

void NamedPipeServer::ExecuteCurrentState(DWORD pipeIndex) {
  // Use the current state to determine the next
  // operation.
  switch (mPipe[pipeIndex].mCurrentState) {
    case PipeStates::READING:
      // The pipe instance is connected to the client
      // and is ready to read a request from the client.
      ReadRequestMessage(pipeIndex);
      break;
    case PipeStates::WRITING:
      // Evalute the incoming request
      EvaluateAndRespondToRequestMessage(pipeIndex);
      // The request was successfully read from the client.
      // Get the reply data and write it to the client.
      WriteReplyMessage(pipeIndex);
      break;
    case PipeStates::CONNECTING:
      // We only end up in a connecting state if the client
      // closes the pipe and we created a new one. This is fine
      break;
    default:
      break;
  }

  return;
}

void NamedPipeServer::ReadRequestMessage(DWORD pipeIndex) {
  BOOL success = ReadFile(
      mPipe[pipeIndex].mPipeInstance.get(), mPipe[pipeIndex].mPipeReadBuffer,
      MaxBufferSize, &mPipe[pipeIndex].mBytesRead, &mPipe[pipeIndex].mOverlap);

  if (success && mPipe[pipeIndex].mBytesRead != 0) {
    // The read operation completed successfully.

    // Copy the data to the request memory buffer for processing.
    mPipe[pipeIndex].mRequestBuffer->AddItem(mPipe[pipeIndex].mPipeReadBuffer,
                                             mPipe[pipeIndex].mBytesRead);

    // We have no pending IO so set our next state to writing to
    // send a reply to the request.
    mPipe[pipeIndex].mPendingIO = FALSE;
    mPipe[pipeIndex].mCurrentState = PipeStates::WRITING;
    return;
  }

  DWORD lastError = GetLastError();
  if (!success &&
      ((lastError == ERROR_IO_PENDING) || lastError == ERROR_MORE_DATA)) {
    // The read operation is still pending or there is more data to be
    // read in the pipe. Set the pending IO flag so that we attempt to read
    // from the pipe again later.
    mPipe[pipeIndex].mPendingIO = TRUE;
    return;
  }

  // An error occurred; disconnect from the client.
  DisconnectAndReconnect(pipeIndex);

  return;
}

void NamedPipeServer::WriteReplyMessage(DWORD pipeIndex) {
  DWORD bytesWritten;
  DWORD expectedBytesWritten;
  BOOL success;

  success = WriteFile(
      mPipe[pipeIndex].mPipeInstance.get(),
      mPipe[pipeIndex].mResponseBuffer->AccessMem(),
      static_cast<DWORD>(mPipe[pipeIndex].mResponseBuffer->GetCurrentSize()),
      &bytesWritten, &mPipe[pipeIndex].mOverlap);
  expectedBytesWritten =
      static_cast<DWORD>(mPipe[pipeIndex].mResponseBuffer->GetCurrentSize());

  if (success && bytesWritten == expectedBytesWritten) {
    // The write operation completed successfully. Set our current
    // state to be reading to be ready for the next incoming
    // request.
    mPipe[pipeIndex].mPendingIO = FALSE;
    mPipe[pipeIndex].mCurrentState = PipeStates::READING;
    mPipe[pipeIndex].mRequestBuffer->ClearMemory();
    mPipe[pipeIndex].mResponseBuffer->ClearMemory();
    return;
  }

  DWORD lastError = GetLastError();
  if (!success && (lastError == ERROR_IO_PENDING)) {
    // The write operation is still pending. Set the pending
    // IO flag so that we attempt to write to the pipe again
    // later.
    mPipe[pipeIndex].mPendingIO = TRUE;
    return;
  }

  // An error occurred; disconnect from the client.
  DisconnectAndReconnect(pipeIndex);

  return;
}

void NamedPipeServer::EvaluateAndRespondToRequestMessage(DWORD pipeIndex) {
  ProcessRequests(mPm, mPipe[pipeIndex].mRequestBuffer.get(),
                  mPipe[pipeIndex].mResponseBuffer.get());
  return;
}

DWORD NamedPipeServer::GetNumActiveConnections() {
  DWORD numActiveClients = 0;

  for (DWORD i = 0; i < mMaxPipes; i++) {
    if (mPipe[i].mCurrentState == PipeStates::CONNECTING &&
        mPipe[i].mPendingIO == TRUE) {
      continue;
    } else {
      numActiveClients++;
    }
  }

  return numActiveClients;
}