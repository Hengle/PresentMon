// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "HResult.h"
#include <format>
#include <Core/source/win/WinAPI.h>
#include <Core/Third/dxerr/dxerr.h>
#include <Core/source/infra/svc/Services.h>


namespace p2c::infra::util::errtl
{
	namespace {
		HRESULT Get(const ErrorCode& code) { return std::any_cast<HRWrap>(code).val; }
	}

	void HResult::RegisterService()
	{
		infra::svc::Services::Singleton<infra::util::ErrorCodeTranslator>(
			typeid(infra::util::errtl::HRWrap).name(),
			[] { return std::make_shared<infra::util::errtl::HResult>(); }
		);
	}

	std::wstring HResult::GetSymbol(const ErrorCode& code) const
	{
		return std::format(L"{:#X}", Get(code));
	}

	std::wstring HResult::GetName(const ErrorCode& code) const
	{
		return DXGetErrorStringW(Get(code));
	}

	std::wstring HResult::GetDescription(const ErrorCode& code) const
	{
		wchar_t buffer[512];
		DXGetErrorDescriptionW(Get(code), buffer, std::size(buffer));
		std::wstring desc = buffer;
		if (const auto pos = desc.find_last_not_of(L" \r\n\t"); pos != std::string::npos)
		{
			desc.resize(pos + 1);
		}
		else
		{
			desc = {};
		}
		return desc;
	}
}
