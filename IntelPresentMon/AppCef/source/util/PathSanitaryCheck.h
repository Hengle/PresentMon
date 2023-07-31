// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <filesystem>

namespace p2c::client::util
{
	bool PathSanitaryCheck(const std::filesystem::path& path, const std::filesystem::path& root);
}