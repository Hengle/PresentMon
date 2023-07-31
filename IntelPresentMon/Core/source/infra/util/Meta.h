// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <concepts>
#include <optional>

namespace p2c::infra::util
{
	template <typename T>
	concept is_optional = std::same_as<T, std::optional<typename T::value_type>>;
}