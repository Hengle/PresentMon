// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "ErrorCode.h"
#include <Core/source/infra/svc/Services.h>
#include <format>

namespace p2c::infra::util
{
	// Error Code Translator

	std::wstring ErrorCodeTranslator::GetFormatted(const ErrorCode& code) const
	{
		return std::format(L"CODE<{}:{}:{}> ", GetSymbol(code), GetName(code), GetDescription(code));
	}


	// ErrorCode

	std::shared_ptr<ErrorCodeTranslator> ErrorCode::GetTranslator() const
	{
		if (!pCachedTranslator)
		{
			pCachedTranslator = svc::Services::ResolveOrNull<ErrorCodeTranslator>({}, code.type().name());
		}
		return *pCachedTranslator;
	}

	std::optional<long long> ErrorCode::GetIntegralView() const { return intView; }

	ErrorCode::operator const std::any&() const
	{
		return code;
	}

	std::wstring ErrorCode::GetSymbol() const
	{
		if (auto pt = GetTranslator())
		{
			return pt->GetSymbol(*this);
		}
		else if (auto view = GetIntegralView())
		{
			return std::to_wstring(*view);
		}
		return {};
	}

	std::wstring ErrorCode::GetName() const
	{
		if (auto pt = GetTranslator())
		{
			return pt->GetName(*this);
		}
		return {};
	}

	std::wstring ErrorCode::GetDescription() const
	{
		if (auto pt = GetTranslator())
		{
			return pt->GetDescription(*this);
		}
		return {};
	}

	std::wstring ErrorCode::GetFormatted() const
	{
		if (auto pt = GetTranslator())
		{
			return pt->GetFormatted(*this);
		}
		else if (auto view = GetIntegralView())
		{
			return std::format(L"CODE~[{}]", *view);
		}
		return L"CODE=??";
	}

	bool ErrorCode::HasTranslator() const
	{
		return (bool)GetTranslator();
	}
}