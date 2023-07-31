// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "PMStatus.h"
#include <format>
#include <Core/source/win/WinAPI.h>
#include <Core/Third/dxerr/dxerr.h>
#include <Core/source/infra/svc/Services.h>

#define PM_STATUS_X_LIST \
	X_(PM_STATUS_SUCCESS) \
	X_(PM_STATUS_CREATE_SESSION_FAILED) \
	X_(PM_STATUS_NO_DATA) \
	X_(PM_STATUS_DATA_LOSS) \
	X_(PM_STATUS_INSUFFICIENT_BUFFER) \
	X_(PM_STATUS_INVALID_SESSION) \
	X_(PM_STATUS_SESSION_ALREADY_EXISTS) \
	X_(PM_STATUS_SERVICE_NOT_INITIALIZED) \
	X_(PM_STATUS_SERVICE_NOT_FOUND) \
	X_(PM_STATUS_SERVICE_SESSIONS_FULL) \
	X_(PM_STATUS_SERVICE_ERROR) \
	X_(PM_STATUS_SERVICE_NOT_SUPPORTED) \
	X_(PM_STATUS_OUT_OF_RANGE) \
	X_(PM_STATUS_INVALID_PID) \
	X_(PM_STATUS_INVALID_ADAPTER_ID) \
	X_(PM_STATUS_INVALID_ETL_FILE) \
	X_(PM_STATUS_PROCESS_NOT_EXIST) \
	X_(PM_STATUS_STREAM_ALREADY_EXISTS) \
	X_(PM_STATUS_UNABLE_TO_CREATE_NSM) \
	X_(PM_STATUS_ERROR)

namespace p2c::infra::util::errtl
{
	namespace {
		PM_STATUS Get(const ErrorCode& code) { return std::any_cast<PM_STATUS>(code); }
	}

	void PMStatus::RegisterService()
	{
		infra::svc::Services::Singleton<infra::util::ErrorCodeTranslator>(
			typeid(PM_STATUS).name(),
			[] { return std::make_shared<infra::util::errtl::PMStatus>(); }
		);
	}

	std::wstring PMStatus::GetSymbol(const ErrorCode& code) const
	{
		return std::to_wstring((int)Get(code));
	}

	std::wstring PMStatus::GetName(const ErrorCode& code) const
	{
		switch (Get(code))
		{
#define X_(name) case PM_STATUS::name: return L###name;
		PM_STATUS_X_LIST
#undef X_
		default: return L"???";
		}
	}

	std::wstring PMStatus::GetDescription(const ErrorCode& code) const
	{
		return L"";
	}
}
