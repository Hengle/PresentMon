// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <utility>


namespace p2c::infra::util
{
	size_t HashCombine(size_t lhs, size_t rhs);

	template <typename T, typename S>
	size_t DualHash(const T& t, const S& s)
	{
		return HashCombine(std::hash<T>{}(t), std::hash<S>{}(s));
	}
}

namespace std
{
	template <class T, class S>
	struct hash<std::pair<T, S>>
	{
		size_t operator()(const std::pair<T, S>& p) const
		{
			return p2c::infra::util::DualHash(p.first, p.second);
		}
	};
}