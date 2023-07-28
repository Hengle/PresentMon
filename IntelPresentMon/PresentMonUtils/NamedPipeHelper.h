#pragma once

#include "../PresentMonService/PresentMon.h"
#include "MemBuffer.h"
#include "PresentMonNamedPipe.h"
#include <string.h>
using namespace std;

class NamedPipeHelper
{
public:
    static void SetServiceError(MemBuffer* rsp_buf, IPMSMResponseHeader& rsp_header);
    static void PopulateRequestHeader(
        IPMSMRequestHeader& rqstHeader,
        PM_ACTION action,
        DWORD version,
        DWORD payloadSize);

    static PM_STATUS EncodeStartStreamingRequest(MemBuffer* rqst_buf,
                                                 uint32_t client_process_id,
                                                 uint32_t target_process_id,
                                                 char const* etl_file_name);
    static PM_STATUS DecodeStartStreamingResponse(
        MemBuffer* rsp_buf, IPMSMStartStreamResponse* start_stream_response);

    static PM_STATUS EncodeStopStreamingRequest(MemBuffer* rqst_buf,
                                                uint32_t client_process_id,
                                                uint32_t target_process_id);
    static PM_STATUS DecodeStopStreamingResponse(MemBuffer* rsp_buf);

    static PM_STATUS EncodeRequestHeader(MemBuffer* rqst_buf,
                                         PM_ACTION pm_action);
    static PM_STATUS DecodeEnumerateAdaptersResponse(
        MemBuffer* rsp_buf, IPMAdapterInfo* adapter_info);

    static PM_STATUS EncodeGeneralSetActionRequest(PM_ACTION action,
                                                   MemBuffer* rqst_buf,
                                                   uint32_t value);
    static PM_STATUS DecodeGeneralSetActionResponse(PM_ACTION action,
                                                   MemBuffer* rsp_buffer);
    static PM_STATUS DecodeCpuNameResponse(MemBuffer* rsp_buf,
                                           IPMCpuNameResponse* cpu_name_info);

    static bool ValidateRequest(MemBuffer* rqst_buf, PM_ACTION action);

    static IPMSMResponseHeader* GetResponseHeader(MemBuffer* rsp_buf);
    static const IPMSMGeneralRequestInfo* GetGeneralRequestInfo(MemBuffer* rqst_buf, PM_ACTION action);
    static const IPMSMRequestHeader* GetRequestHeader(MemBuffer* rqst_buf);

    static void PopulateResponseHeader(
        IPMSMResponseHeader& rsp_header,
        PM_ACTION action,
        DWORD version,
        DWORD payload_size,
        PM_STATUS result);

private:

    static bool ValidateResponse(MemBuffer* rsp_buf, PM_ACTION action);
    static const BYTE* GetResponsePayloadPtr(MemBuffer* rsp_buf, PM_ACTION action);

    static void UpdateResponseHeaderPayloadSize(MemBuffer* buffer, IPMSMResponseHeader& rsp_header, DWORD payload_size);
    static bool ValidatePayloadRequestSize(PM_ACTION action, DWORD header_payload_size, MemBuffer* rqst_buf);
    static bool ValidatePayloadResponseSize(PM_ACTION action, DWORD header_payload_size, MemBuffer* rsp_buf);
};