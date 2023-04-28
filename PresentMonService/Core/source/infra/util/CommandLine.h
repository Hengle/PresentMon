// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <vector>
#include <string>
#include "Util.h"

namespace p2c::infra::util
{
	class CommandLine
	{
	public:
		CommandLine();
		CommandLine(std::string commandLine);
		const std::string& Raw() const;
		const std::vector<std::string>& Parsed() const;
		std::string FindOption(const std::string& needle) const;
	private:
		std::string raw;
		std::vector<std::string> parsed;
	};
}