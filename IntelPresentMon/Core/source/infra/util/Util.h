// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <string_view>
#include <string>
#include <vector>

namespace p2c::infra::util
{
	std::vector<std::string> TokenizeQuoted(const std::string& input);
	std::wstring ToWide(const std::string& narrow);
	std::string ToNarrow(const std::wstring& wide);
}