// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <optional>


namespace p2c::client::util
{
	class CefProcessCompass
	{
	public:
		CefProcessCompass();
		const std::optional<std::string>& GetType() const;
		bool IsClient() const;
	private:
		std::optional<std::string> type;
	};
}