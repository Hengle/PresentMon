// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "CefProcessCompass.h"
#include <Core/source/infra/svc/Services.h>
#include <Core/source/infra/util/CommandLine.h>

namespace p2c::client::util
{
	CefProcessCompass::CefProcessCompass()
	{
		auto pLine = infra::svc::Services::Resolve<infra::util::CommandLine>();
		if (auto type_ = pLine->FindOption("type"); !type_.empty())
		{
			type = std::move(type_);
		}
	}

	const std::optional<std::string>& CefProcessCompass::GetType() const
	{
		return type;
	}

	bool CefProcessCompass::IsClient() const
	{
		return type.has_value();
	}
}
