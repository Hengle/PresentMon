// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "Util.h"
#include <stdlib.h>
#include <sstream>
#include <iomanip>

namespace p2c::infra::util
{
	std::vector<std::string> TokenizeQuoted(const std::string& input)
	{
		std::istringstream stream;
		stream.str(input);
		std::vector<std::string> tokens;
		std::string token;

// okay to move from token and use because std::quoted(token) fills the string and does not read from
#pragma warning(push)
#pragma warning(disable : 26800)
		while (stream >> std::quoted(token))
		{
			tokens.push_back(std::move(token));
		}
#pragma warning(pop)
		return tokens;
	}

	std::wstring ToWide(const std::string& narrow)
	{
		std::wstring wide;
		wide.resize(narrow.size() + 1);
		size_t actual;
		mbstowcs_s(&actual, wide.data(), wide.size(), narrow.c_str(), _TRUNCATE);
		if (actual > 0)
		{
			wide.resize(actual - 1);
			return wide;
		}
		return {};
	}

	std::string ToNarrow(const std::wstring& wide)
	{
		std::string narrow;
		narrow.resize(wide.size() * 2);
		size_t actual;
		wcstombs_s(&actual, narrow.data(), narrow.size(), wide.c_str(), _TRUNCATE);
		narrow.resize(actual - 1);
		return narrow;
	}
}