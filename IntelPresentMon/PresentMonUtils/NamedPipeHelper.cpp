// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "NamedPipeHelper.h"

// Function to populate a response header
void NamedPipeHelper::PopulateResponseHeader(IPMSMResponseHeader& rsp_header,
                                             PM_ACTION action, DWORD version,
                                             DWORD payload_size,
                                             PM_STATUS result) {
  // The response header always starts with the id string
  _tcscpy_s(rsp_header.idString, 6, ipmsmIdString);
  rsp_header.action = action;
  rsp_header.version = version;
  rsp_header.payloadSize = payload_size;
  rsp_header.result = result;
}

// Function to populate a response header
void NamedPipeHelper::PopulateRequestHeader(IPMSMRequestHeader& rqstHeader,
                                            PM_ACTION action, DWORD version,
                                            DWORD payloadSize) {
  // The request header always starts with the id string
  _tcscpy_s(rqstHeader.idString, 6, ipmsmIdString);
  rqstHeader.version = version;
  rqstHeader.action = action;
  rqstHeader.payloadSize = payloadSize;
}

// Function to update the payload size of a response header that has already
// been added to a memory buffer.
void NamedPipeHelper::UpdateResponseHeaderPayloadSize(
    MemBuffer* buffer, IPMSMResponseHeader& rsp_header, DWORD payload_size) {
  (void) rsp_header;
  // The response header is the very first item in the memory buffer.
  IPMSMResponseHeader* currentResponse =
      static_cast<IPMSMResponseHeader*>(buffer->AccessMem());
  currentResponse->payloadSize = payload_size;
}

bool NamedPipeHelper::ValidatePayloadRequestSize(PM_ACTION action,
                                                 DWORD header_payload_size,
                                                 MemBuffer* rqst_buf) {
  switch (action) {
    case PM_ACTION::START_STREAM:
    case PM_ACTION::STOP_STREAM:
    case PM_ACTION::SELECT_ADAPTER:
    case PM_ACTION::SET_GPU_TELEMETRY_PERIOD: {
      // Check the header value
      if ((header_payload_size != sizeof(IPMSMGeneralRequestInfo))) {
        return false;
      }
      // Check if the size of the buffer matches the payload size
      size_t bufPayloadSize =
          rqst_buf->GetCurrentSize() - sizeof(IPMSMRequestHeader);
      if (bufPayloadSize != +sizeof(IPMSMGeneralRequestInfo)) {
        return false;
      }
      break;
    }
    case PM_ACTION::ENUMERATE_ADAPTERS:
    case PM_ACTION::GET_CPU_NAME: {
      // Enumeration of GPUs and getting the CPU name only requires the request
      // header.
      if ((header_payload_size != 0)) {
        return false;
      }
      // Check if the size of the buffer matches the sizes of the request
      // header
      if (rqst_buf->GetCurrentSize() != sizeof(IPMSMRequestHeader)) {
        return false;
      }
      break;
    }
    default:
      return false;
  }

  return true;
}

bool NamedPipeHelper::ValidatePayloadResponseSize(PM_ACTION action,
                                                  DWORD header_payload_size,
                                                  MemBuffer* rsp_buf) {
  switch (action) {
    case PM_ACTION::START_STREAM: {
      // The start streaming response payload consists of a single
      // IPMSMStartStreamResponse struct Check the header value
      if (header_payload_size != sizeof(IPMSMStartStreamResponse)) {
        return false;
      }

      // Check if the size of the buffer matches the payload size
      size_t bufPayloadSize =
          rsp_buf->GetCurrentSize() - sizeof(IPMSMResponseHeader);
      if (header_payload_size != bufPayloadSize) {
        return false;
      }
      return true;
    }
    case PM_ACTION::STOP_STREAM:
    case PM_ACTION::SELECT_ADAPTER:
    case PM_ACTION::SET_GPU_TELEMETRY_PERIOD: {
      // The stop streaming and select adapter responses consists of header
      // only. Check if the size of the buffer matches the payload size
      size_t bufPayloadSize =
          rsp_buf->GetCurrentSize() - sizeof(IPMSMResponseHeader);
      if (header_payload_size != bufPayloadSize) {
        return false;
      }

      return true;
    }
    case PM_ACTION::ENUMERATE_ADAPTERS: {
      // The emumerate adapters response consists of the header and
      // returned gpu data.
      size_t bufPayloadSize =
          rsp_buf->GetCurrentSize() - (sizeof(IPMSMResponseHeader));
      if (header_payload_size != bufPayloadSize) {
        return false;
      }
      return true;
    }
    case PM_ACTION::GET_CPU_NAME: {
      // The get cpu name response consists of the header and
      // returned cpu name response structure
      if (rsp_buf->GetCurrentSize() !=
          sizeof(IPMSMResponseHeader) + sizeof(IPMCpuNameResponse)){
        return false;      
      }
      size_t bufPayloadSize =
          rsp_buf->GetCurrentSize() - (sizeof(IPMSMResponseHeader));
      if (header_payload_size != bufPayloadSize) {
        return false;
      }
      return true;
    }
  }
  return false;
}

bool NamedPipeHelper::ValidateRequest(MemBuffer* rqst_buf, PM_ACTION action) {
  // First verify the request has the correct idString and
  // appropriate version
  const IPMSMRequestHeader* request = GetRequestHeader(rqst_buf);
  if (_tcscmp(request->idString, ipmsmIdString) != 0 && request->version != 1) {
    // The incomping message was not properly formed
    return false;
  }

  // Now validate payload size
  return ValidatePayloadRequestSize(action, request->payloadSize, rqst_buf);
}

bool NamedPipeHelper::ValidateResponse(MemBuffer* rsp_buf, PM_ACTION action) {
  // First verify the request has the correct idString and
  // appropriate version
  const IPMSMResponseHeader* response = GetResponseHeader(rsp_buf);
  if (_tcscmp(response->idString, ipmsmIdString) != 0 &&
      response->version != 1) {
    // The incomping message was not properly formed
    return false;
  }

  // Now validate payload size
  return ValidatePayloadResponseSize(action, response->payloadSize, rsp_buf);
}

const IPMSMGeneralRequestInfo* NamedPipeHelper::GetGeneralRequestInfo(
    MemBuffer* rqst_buf, PM_ACTION action) {
  BYTE* rspBufPtr = static_cast<BYTE*>(rqst_buf->AccessMem());

  switch (action) {
    case PM_ACTION::START_STREAM:
    case PM_ACTION::STOP_STREAM:
    case PM_ACTION::SELECT_ADAPTER:
    case PM_ACTION::SET_GPU_TELEMETRY_PERIOD: {
      // Move the pointer to the general response header
      return reinterpret_cast<IPMSMGeneralRequestInfo*>(
          (rspBufPtr + sizeof(IPMSMRequestHeader)));
      break;
    }
    default:
      return nullptr;
  }
}

IPMSMResponseHeader* NamedPipeHelper::GetResponseHeader(MemBuffer* rqst_buf) {
  // The request header should ALWAYS be at the top of the response buffer.
  IPMSMResponseHeader* responseHeader =
      static_cast<IPMSMResponseHeader*>(rqst_buf->AccessMem());

  return responseHeader;
}

const BYTE* NamedPipeHelper::GetResponsePayloadPtr(MemBuffer* rqst_buf,
                                                   PM_ACTION action) {
  BYTE* payloadPtr = static_cast<BYTE*>(rqst_buf->AccessMem());

  switch (action) {
    case PM_ACTION::START_STREAM:
    case PM_ACTION::ENUMERATE_ADAPTERS: 
    case PM_ACTION::GET_CPU_NAME: {
      payloadPtr = payloadPtr + sizeof(IPMSMResponseHeader);
    } break;

    default:
      payloadPtr = nullptr;
  }

  return payloadPtr;
}

void NamedPipeHelper::SetServiceError(MemBuffer* rqst_buf,
                                      IPMSMResponseHeader& rqst_header) {
  // Get the incoming request pointer
  const IPMSMRequestHeader* request = GetRequestHeader(rqst_buf);

  PopulateResponseHeader(rqst_header, request->action, 1, 0,
                         PM_STATUS::PM_STATUS_SERVICE_ERROR);

  return;
}

const IPMSMRequestHeader* NamedPipeHelper::GetRequestHeader(
    MemBuffer* rqst_buf) {
  // The request header should ALWAYS be at the top of the request buffer.
  IPMSMRequestHeader* requestHeader =
      static_cast<IPMSMRequestHeader*>(rqst_buf->AccessMem());

  return requestHeader;
}

// Streams requests can either be for a currently running process OR from
// an ETW log file
PM_STATUS NamedPipeHelper::EncodeStartStreamingRequest(
    MemBuffer* rqst_buf, uint32_t client_process_id, uint32_t target_process_id,
    char const* etl_file_name) {
  IPMSMRequestHeader request;

  PopulateRequestHeader(request, PM_ACTION::START_STREAM, 1,
                        sizeof(IPMSMGeneralRequestInfo));

  rqst_buf->AddItem(&request, sizeof(request));

  IPMSMGeneralRequestInfo gen_request_info{};
  if (etl_file_name) {
    std::string local_etl_file_name = etl_file_name;
    if (local_etl_file_name.size() == 0 ||
        local_etl_file_name.size() >= MAX_PATH) {
      return PM_STATUS::PM_STATUS_INVALID_ETL_FILE;
    }
    local_etl_file_name.copy(gen_request_info.etlFileName,
                             sizeof(gen_request_info.etlFileName));
    gen_request_info.etlFileNameLength = local_etl_file_name.size();
    gen_request_info.clientProcessId = client_process_id;
  } else {
    gen_request_info.clientProcessId = client_process_id;
    gen_request_info.targetProcessId = target_process_id;
  }

  rqst_buf->AddItem(&gen_request_info, sizeof(gen_request_info));

  return PM_STATUS::PM_STATUS_SUCCESS;
}

PM_STATUS NamedPipeHelper::DecodeStartStreamingResponse(
    MemBuffer* rqst_buf, IPMSMStartStreamResponse* start_stream_response) {
  if (!ValidateResponse(rqst_buf, PM_ACTION::START_STREAM)) {
    return PM_STATUS::PM_STATUS_SERVICE_ERROR;
  }

  const IPMSMResponseHeader* response = GetResponseHeader(rqst_buf);
  if (response->result == PM_STATUS::PM_STATUS_SUCCESS) {
    memcpy_s(start_stream_response, sizeof(IPMSMStartStreamResponse),
             GetResponsePayloadPtr(rqst_buf, PM_ACTION::START_STREAM),
             sizeof(IPMSMStartStreamResponse));
  }

  return response->result;
}

PM_STATUS NamedPipeHelper::EncodeStopStreamingRequest(
    MemBuffer* rqst_buf, uint32_t client_process_id,
    uint32_t target_process_id) {
  IPMSMRequestHeader request;

  PopulateRequestHeader(request, PM_ACTION::STOP_STREAM, 1,
                        sizeof(IPMSMGeneralRequestInfo));

  rqst_buf->AddItem(&request, sizeof(request));

  IPMSMGeneralRequestInfo gen_request_info;
  ZeroMemory(&gen_request_info, sizeof(gen_request_info));
  gen_request_info.clientProcessId = client_process_id;
  gen_request_info.targetProcessId = target_process_id;
  rqst_buf->AddItem(&gen_request_info, sizeof(gen_request_info));

  return PM_STATUS::PM_STATUS_SUCCESS;
}

PM_STATUS NamedPipeHelper::DecodeStopStreamingResponse(MemBuffer* rqst_buf) {
  if (!ValidateResponse(rqst_buf, PM_ACTION::STOP_STREAM)) {
    return PM_STATUS::PM_STATUS_SERVICE_ERROR;
  }

  const IPMSMResponseHeader* response = GetResponseHeader(rqst_buf);

  return response->result;
}

PM_STATUS NamedPipeHelper::EncodeRequestHeader(MemBuffer* rqst_buf,
                                               PM_ACTION pm_action) {
  IPMSMRequestHeader request;

  PopulateRequestHeader(request, pm_action, 1, 0);
  rqst_buf->AddItem(&request, sizeof(request));
  return PM_STATUS::PM_STATUS_SUCCESS;
}

PM_STATUS NamedPipeHelper::DecodeEnumerateAdaptersResponse(
    MemBuffer* rsp_buf, IPMAdapterInfo* adapter_info) {
  if (!ValidateResponse(rsp_buf, PM_ACTION::ENUMERATE_ADAPTERS)) {
    return PM_STATUS::PM_STATUS_SERVICE_ERROR;
  }

  const IPMSMResponseHeader* response = GetResponseHeader(rsp_buf);
  if (response->result == PM_STATUS::PM_STATUS_SUCCESS) {
    memcpy_s(adapter_info, sizeof(IPMAdapterInfo),
             GetResponsePayloadPtr(rsp_buf, PM_ACTION::ENUMERATE_ADAPTERS),
             sizeof(IPMAdapterInfo));
  }

  return response->result;
}

PM_STATUS NamedPipeHelper::EncodeGeneralSetActionRequest(PM_ACTION action,
                                                         MemBuffer* rqst_buf,
                                                         uint32_t value) {
  IPMSMRequestHeader request;

  PopulateRequestHeader(request, action, 1, sizeof(IPMSMGeneralRequestInfo));
  rqst_buf->AddItem(&request, sizeof(request));
  IPMSMGeneralRequestInfo gen_request_info;
  ZeroMemory(&gen_request_info, sizeof(gen_request_info));
  switch (action) {
    case PM_ACTION::SELECT_ADAPTER:
      gen_request_info.adapterId = value;
      break;
    case PM_ACTION::SET_GPU_TELEMETRY_PERIOD:
      gen_request_info.gpuTelemetrySamplePeriodMs = value;
      break;
  }
  rqst_buf->AddItem(&gen_request_info, sizeof(gen_request_info));

  return PM_STATUS::PM_STATUS_SUCCESS;
}

PM_STATUS NamedPipeHelper::DecodeGeneralSetActionResponse(
    PM_ACTION action, MemBuffer* rsp_buffer) {
  if (!ValidateResponse(rsp_buffer, action)) {
    return PM_STATUS::PM_STATUS_SERVICE_ERROR;
  }
  const IPMSMResponseHeader* response = GetResponseHeader(rsp_buffer);
  return response->result;
}

PM_STATUS NamedPipeHelper::DecodeCpuNameResponse(
    MemBuffer* rsp_buf, IPMCpuNameResponse* cpu_name_info) {
  if (!ValidateResponse(rsp_buf, PM_ACTION::GET_CPU_NAME)) {
    return PM_STATUS::PM_STATUS_SERVICE_ERROR;
  }

  const IPMSMResponseHeader* response = GetResponseHeader(rsp_buf);
  if (response->result == PM_STATUS::PM_STATUS_SUCCESS) {
    memcpy_s(cpu_name_info, sizeof(IPMCpuNameResponse),
             GetResponsePayloadPtr(rsp_buf, PM_ACTION::GET_CPU_NAME),
             sizeof(IPMCpuNameResponse));
  }

  return response->result;
}
