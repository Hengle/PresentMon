// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <memory>
#include <Core/source/kernel/OverlaySpec.h>
#include <include/cef_values.h>

namespace p2c::kern
{
	struct OverlaySpec;
}

namespace p2c::client::util
{
	std::unique_ptr<kern::OverlaySpec> MakeOverlaySpec(CefRefPtr<CefValue> vSpec);
}