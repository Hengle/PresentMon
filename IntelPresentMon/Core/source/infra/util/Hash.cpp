// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "Hash.h"


namespace p2c::infra::util
{
	size_t HashCombine(size_t lhs, size_t rhs)
	{
		return lhs ^ (rhs + 0x517c'c1b7'2722'0a95 + (lhs << 6) + (lhs >> 2));
	}
}