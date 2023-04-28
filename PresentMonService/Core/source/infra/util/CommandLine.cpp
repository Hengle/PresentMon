// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "CommandLine.h"
#include <Core/source/win/WinAPI.h>

namespace p2c::infra::util
{
	CommandLine::CommandLine()
		:
		CommandLine{ GetCommandLineA() }
	{}

	CommandLine::CommandLine(std::string commandLine)
		:
		raw{ std::move(commandLine) },
		parsed{ TokenizeQuoted(raw) }
	{}

	const std::string& CommandLine::Raw() const
	{
		return raw;
	}

	const std::vector<std::string>& CommandLine::Parsed() const
	{
		return parsed;
	}

	std::string CommandLine::FindOption(const std::string& needle) const
	{
		const auto optionString = "--" + needle;
		const auto i = std::find(parsed.begin(), parsed.end(), optionString);

		if (i == parsed.end())
		{
			return "";
		}

		const auto next = std::next(i);

		if (next == parsed.end() || next->substr(0, 2) == "--")
		{
			return needle;
		}

		return *next;
	}
}