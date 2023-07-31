/*
 *
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 *
 * File Name:  PresentMonNamedPipe.h
 *
 * Abstract:   Header for Intel PresentMon Named Pipe IPC
 *
 * Notes:      File to be used for communication with the Intel PresentMon
 *             Service IPC.
 */
#pragma once
#include <Windows.h>
#include <tchar.h>
#include <bitset>
#include "..\..\PresentData\PresentMonTraceConsumer.hpp"
#include "..\PresentMonAPI\PresentMonAPI.h"
#include "..\ControlLib\PowerTelemetryProvider.h"
#include "..\ControlLib\CpuTelemetryInfo.h"

// We use system reserved pid (0: System Idle Process, 4: System) that will
// never show up in present mon for StreamAll and ETL PIDs
enum class StreamPidOverride : uint32_t { kStreamAllPid = 0, kEtlPid = 4 };

#ifdef __cplusplus
extern "C" {
#endif

enum class PM_ACTION
{
	START_STREAM,
	STOP_STREAM,
	PROCESS_ETL,
	ENUMERATE_ADAPTERS,
	SELECT_ADAPTER,
	SET_GPU_TELEMETRY_PERIOD,
	GET_CPU_NAME,
	INVALID_REQUEST
};

#define MAX_PM_ADAPTERS 8

// Hardcoded identifier string used in
// IPC communication for the PresentMon Service
static LPCTSTR ipmsmIdString = TEXT("IPMSM");

// Definition of the Intel Present Mon Service Request Header
struct IPMSMRequestHeader
{
	TCHAR       idString[6];
	DWORD       version;
	PM_ACTION	action;
	DWORD       payloadSize;
};

// Definition of the Intel Present Mon Service Response Header
struct IPMSMResponseHeader
{
	TCHAR       idString[6];
	DWORD       version;
	PM_ACTION	action;
	PM_STATUS   result;
	DWORD       payloadSize;
};

struct IPMSMGeneralRequestInfo
{
	uint32_t    clientProcessId;
	uint32_t    targetProcessId;
	uint32_t	adapterId;
	uint32_t    gpuTelemetrySamplePeriodMs;
    char        etlFileName[MAX_PATH];
    size_t      etlFileNameLength;
};

struct NamedSharedMemoryHeader {
  NamedSharedMemoryHeader()
      : start_qpc(0),
	  last_displayed_qpc(0),
	  buf_size(0),
	  max_entries(0),
	  current_write_offset(0),
	  num_frames_written(0),
	  head_idx(0),
	  tail_idx(0),
      process_active(true){};
  // start QPC time of the very first frame recorderd after PmStartStream
  char application[MAX_PATH] = {};
  uint64_t start_qpc;
  LARGE_INTEGER qpc_frequency = {};
  uint64_t last_displayed_qpc;
  uint64_t buf_size;
  uint64_t max_entries;
  uint64_t current_write_offset;
  uint64_t num_frames_written;
  uint64_t head_idx;
  uint64_t tail_idx;
  bool process_active;
  std::bitset<static_cast<size_t>(GpuTelemetryCapBits::gpu_telemetry_count)>
      gpuTelemetryCapBits{};
  std::bitset<static_cast<size_t>(CpuTelemetryCapBits::cpu_telemetry_count)>
      cpuTelemetryCapBits{};
};

struct PmNsmPresentEvent {
  uint64_t PresentStartTime;  // QPC value of the first event related to the
                              // Present (D3D9, DXGI, or DXGK Present_Start)
  uint32_t ProcessId;         // ID of the process that presented
  uint32_t ThreadId;          // ID of the thread that presented
  uint64_t
      PresentStopTime;    // QPC duration between runtime present start and end
  uint64_t GPUStartTime;  // QPC value when the frame's first DMA packet started
  uint64_t ReadyTime;    // QPC value when the frame's last DMA packet completed

  uint64_t GPUDuration;  // QPC duration during which a frame's DMA packet was
                         // running on any node
  uint64_t
      GPUVideoDuration;  // QPC duration during which a frame's DMA packet was
                         // running on a video node (if mTrackGPUVideo==true)
  uint64_t ScreenTime;   // QPC value when the present was displayed on screen

  uint64_t InputTime;  // Earliest QPC value when the keyboard/mouse was clicked
                       // and used by this frame

  // Extra present parameters obtained through DXGI or D3D9 present
  uint64_t SwapChainAddress;
  int32_t SyncInterval;
  uint32_t PresentFlags;

  // Keys used to index into PMTraceConsumer's tracking data structures:
  uint64_t CompositionSurfaceLuid;   // mPresentByWin32KPresentHistoryToken
  uint64_t Win32KPresentCount;       // mPresentByWin32KPresentHistoryToken
  uint64_t Win32KBindId;             // mPresentByWin32KPresentHistoryToken
  uint64_t DxgkPresentHistoryToken;  // mPresentByDxgkPresentHistoryToken
  uint64_t
      DxgkPresentHistoryTokenData;     // mPresentByDxgkPresentHistoryTokenData
  uint64_t DxgkContext;                // mPresentByDxgkContext
  uint64_t Hwnd;                       // mLastPresentByWindow
  uint32_t QueueSubmitSequence;        // mPresentBySubmitSequence
  uint32_t mAllPresentsTrackingIndex;  // mAllPresents.

  // How many PresentStop events from the thread to wait for before
  // enqueueing this present.
  uint32_t DeferredCompletionWaitCount;

  // Properties deduced by watching events through present pipeline
  uint32_t DestWidth;
  uint32_t DestHeight;
  uint32_t DriverThreadId;

  Runtime Runtime;
  PresentMode PresentMode;
  PresentResult FinalState;
  InputDeviceType InputType;

  bool SupportsTearing;
  bool WaitForFlipEvent;
  bool WaitForMPOFlipEvent;
  bool SeenDxgkPresent;
  bool SeenWin32KEvents;
  bool DwmNotified;
  bool SeenInFrameEvent;   // This present has gotten a Win32k TokenStateChanged
                           // event into InFrame state
  bool GpuFrameCompleted;  // This present has already seen an event that caused
                           // GpuTrace::CompleteFrame() to be called.
  bool IsCompleted;        // All expected events have been observed
  bool IsLost;  // This PresentEvent was found in an unexpected state or is too
                // old

  // Whether this PresentEvent is currently stored in
  // PMTraceConsumer::mPresentsWaitingForDWM
  bool PresentInDwmWaitingStruct;

  // QPC time of last presented frame
  uint64_t last_present_qpc;
  // QPC time of the last displayed frame
  uint64_t last_displayed_qpc;

  // Application name
  char application[MAX_PATH];
};

struct PmNsmFrameData {
  PmNsmPresentEvent present_event;
  PresentMonPowerTelemetryInfo power_telemetry;
  CpuTelemetryInfo cpu_telemetry;
};

struct IPMSMStartStreamResponse
{
	bool        enable_file_logging;
	char        fileName[MAX_PATH];
	size_t      fileNameLength;
};

struct IPMAdapterInfo
{
  uint32_t num_adapters;
  PM_ADAPTER_INFO adapters[MAX_PM_ADAPTERS];
};

struct IPMCpuNameResponse
{
  char cpu_name[MAX_PM_CPU_NAME];
  uint32_t cpu_name_length;
};

#ifdef __cplusplus
} // extern "C"
#endif
