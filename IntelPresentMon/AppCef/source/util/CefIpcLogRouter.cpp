// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "CefIpcLogRouter.h"
#include <Core/source/infra/log/EntryOutputBase.h>
#include <Core/source/infra/log/DefaultChannel.h>
#include <Core/source/infra/log/Channel.h>

namespace p2c::client::util::log
{
	void CefIpcLogRouter::Route(std::wstring snapshot)
	{
		infra::log::EntryOutputBase entry;
		entry.snapshot = std::move(snapshot);
		infra::log::GetDefaultChannel()->Accept(entry);
	}
}
