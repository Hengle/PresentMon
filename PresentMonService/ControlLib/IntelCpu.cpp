// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "IntelCpu.h"

namespace pwr::cpu::intel {

#define APP_NAME "PresentMon Service"
#define APP_DESCRIPTION "PresentMon Service IPF Telemetry Provider"
#define PROGRAM_SERVERADDR "pipe://ipfsrv.root"

// Max bias number as defined by graphics driver
static const uint32_t kMaxBias = 0x8000;

static bool g_session_disconnected = false;

// Global callback function
static void ESIF_CALLCONV EventCallback(IpfSession_t ipf_session,
                                        esif_event_type_t event_type,
                                        char* participant_name,
                                        EsifData* data_ptr,
                                        esif_context_t context) {
  if (event_type == ESIF_EVENT_SESSION_DISCONNECTED){
    g_session_disconnected = true;
  }
  return;
}

// public interface functions

IntelCpu::IntelCpu() {

  // Initialize the ipf api
  if (const auto result = IpfCore_Init(); result != ESIF_OK) {
    throw std::runtime_error{"failed ipf api init"};
  }

  auto result = CreateIpfSession();
  if (!result) {
    throw std::runtime_error{"failed ipf session creation"};
  }
}

IntelCpu::~IntelCpu() {
  IpfCore_SessionDisconnect(ipf_session_);
  IpfCore_SessionDestroy(ipf_session_);
  IpfCore_Exit();
}

bool IntelCpu::Sample() noexcept {

  LARGE_INTEGER qpc;
  QueryPerformanceCounter(&qpc);

  CpuTelemetryInfo info{
      .qpc = (uint64_t)qpc.QuadPart,
  };

  // sample cpu temperature
  { 
    char command_buffer[] = "execute-primitive TCPU GET_TEMPERATURE";
    uint32_t response_value = 0;
    EsifData command{.type = ESIF_DATA_STRING,
                     .buf_ptr = command_buffer,
                     .buf_len = sizeof(command_buffer)};
    EsifData response{.type = ESIF_DATA_TEMPERATURE,
                      .buf_ptr = &response_value,
                      .buf_len = sizeof(response_value)};
    if ((SubmitIpfExecuteCommand(&command, nullptr, &response))) {
      info.cpu_temperature = (double)(response_value/10.) - 273.15;
    }
  }

  // sample cpu utilization
  {
    char command_buffer[] =
        "execute-primitive TCPU GET_PARTICIPANT_UTILIZATION D1";
    uint32_t response_value = 0;
    EsifData command{.type = ESIF_DATA_STRING,
                     .buf_ptr = command_buffer,
                     .buf_len = sizeof(command_buffer)};
    EsifData response{.type = ESIF_DATA_PERCENT,
                      .buf_ptr = &response_value,
                      .buf_len = sizeof(response_value)};
    if ((SubmitIpfExecuteCommand(&command, nullptr, &response))) {
      info.cpu_utilization = (double)response_value / 100.;
    }
  }

  // sample cpu power limit
  {
    char command_buffer[] =
        "execute-primitive TCPU GET_RAPL_POWER_LIMIT_1";
    uint32_t response_value = 0;
    EsifData command{.type = ESIF_DATA_STRING,
                     .buf_ptr = command_buffer,
                     .buf_len = sizeof(command_buffer)};
    EsifData response{.type = ESIF_DATA_POWER,
                      .buf_ptr = &response_value,
                      .buf_len = sizeof(response_value)};
    if ((SubmitIpfExecuteCommand(&command, nullptr, &response))) {
      info.cpu_power_limit_w = (double)response_value / 1000.;
    }
  }

  // sample cpu power
  {
    char command_buffer[] = "execute-primitive TCPU GET_RAPL_POWER";
    uint32_t response_value = 0;
    EsifData command{.type = ESIF_DATA_STRING,
                     .buf_ptr = command_buffer,
                     .buf_len = sizeof(command_buffer)};
    EsifData response{.type = ESIF_DATA_POWER,
                      .buf_ptr = &response_value,
                      .buf_len = sizeof(response_value)};
    if ((SubmitIpfExecuteCommand(&command, nullptr, &response))) {
      info.cpu_power_w = (double)response_value / 1000.;
    }
  }

  // sample cpu/dg bias
  {
    char command_buffer[] = "execute-primitive IDG2 GET_SOC_DGPU_WEIGHTS";
    uint32_t response_value = 0;
    EsifData command{.type = ESIF_DATA_STRING,
                     .buf_ptr = command_buffer,
                     .buf_len = sizeof(command_buffer)};
    EsifData response{.type = ESIF_DATA_UINT32,
                      .buf_ptr = &response_value,
                      .buf_len = sizeof(response_value)};
    if ((SubmitIpfExecuteCommand(&command, nullptr, &response))) {
      info.cpu_bias_weight = (double)((0xffff0000 & response_value) >> 16) / kMaxBias;
      info.gpu_bias_weight = (double)(0x0000ffff & response_value) / kMaxBias;
    }
  }

  // insert telemetry into history
  std::lock_guard lock{history_mutex_};
  history_.Push(info);

  return true;
}

// private functions

bool IntelCpu::CreateIpfSession() {
  
  IpfSessionInfo session_info{.v1 = {.revision = IPF_SESSIONINFO_REVISION,
                                     .appName = APP_NAME,
                                     .appVersion = IPF_APP_VERSION,
                                     .appDescription = APP_DESCRIPTION,
                                     .serverAddr = PROGRAM_SERVERADDR
                              }};

  // Create session
  if (ipf_session_ = IpfCore_SessionCreate(&session_info);
      ipf_session_ == IPF_INVALID_SESSION) {
    return false;
  }

  // connect the session to IPF server
  if (const auto result = IpfCore_SessionConnect(ipf_session_);
      result != ESIF_OK) {
    return false;
  }
  
  // Register for disconnect event
  if (const auto result = IpfCore_SessionRegisterEvent(
          ipf_session_, ESIF_EVENT_SESSION_DISCONNECTED, NULL,
          pwr::cpu::intel::EventCallback, NULL);
      result != ESIF_OK) {
    return false;
  }

  return true;
}

bool IntelCpu::SubmitIpfExecuteCommand(EsifData* command,
                                                       EsifData* request,
                                                       EsifData* response) {
  if (pwr::cpu::intel::g_session_disconnected) {
    // according to IPF users guide if the session has been disconnected we
    // need to disconnect and connect the session.
    IpfCore_SessionDisconnect(ipf_session_);
    bool result = CreateIpfSession();
    if (!result) {
      return result;
    }
  }

  if (const auto result =
          IpfCore_SessionExecute(ipf_session_, command, request, response);
      result != ESIF_OK) {
    if (result == ESIF_E_SESSION_DISCONNECTED) {
      // if the call reports back the session has been disconnected set
      // disconneted flag and return false. We'll attempt to reconnect on
      // next sample
      pwr::cpu::intel::g_session_disconnected = true;
      return false;
    }
  }
  return true;
}
  
std::optional<CpuTelemetryInfo> IntelCpu::GetClosest(
    uint64_t qpc) const noexcept {
  std::lock_guard lock{history_mutex_};
  return history_.GetNearest(qpc);
}

}