// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <memory>

namespace p2c::infra::log
{
	class Channel;
	std::shared_ptr<Channel> GetDefaultChannel();
	void SetDefaultChannel(std::shared_ptr<Channel> pNewDefaultChannel);
}