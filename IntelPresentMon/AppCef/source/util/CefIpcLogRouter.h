// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <string>


namespace p2c::client::util::log
{
	class CefIpcLogRouter
	{
	public:
		static void Route(std::wstring snapshot);
		static constexpr const char* ipcChannelName = "log-channel";
	};
}