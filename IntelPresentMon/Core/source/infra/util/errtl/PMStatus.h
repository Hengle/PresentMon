// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <Core/source/infra/util/ErrorCode.h>
#include <Core/source/win/WinAPI.h>
#include <PresentMonAPI/PresentMonAPI.h>

namespace p2c::infra::util::errtl
{
	class PMStatus : public ErrorCodeTranslator
	{
	public:
		static void RegisterService();
		std::wstring GetSymbol(const ErrorCode& code) const override;
		std::wstring GetName(const ErrorCode& code) const override;
		std::wstring GetDescription(const ErrorCode& code) const override;
	};
}