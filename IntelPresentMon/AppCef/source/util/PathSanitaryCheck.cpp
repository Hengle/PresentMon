// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "PathSanitaryCheck.h"
#include <algorithm>

namespace p2c::client::util
{
	bool PathSanitaryCheck(const std::filesystem::path& path, const std::filesystem::path& root)
	{
		const auto canonicalString = std::filesystem::weakly_canonical(path).wstring();
		const auto rootString = root.wstring();
		return canonicalString.starts_with(rootString);
	}
}