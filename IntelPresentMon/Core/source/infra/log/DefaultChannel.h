// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <memory>
#include <functional>

namespace p2c::infra::log
{
	class Channel;
	std::shared_ptr<Channel> GetDefaultChannel();
	void SetDefaultChannel(std::shared_ptr<Channel> pNewDefaultChannel);
	void SetDefaultChannelFactory(std::function<std::shared_ptr<Channel>()> defaultChannelFactory);
}