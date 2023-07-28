// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "DefaultChannel.h"
#include "Channel.h"
#include "drv/SimpleFileDriver.h"
#include "drv/DebugOutDriver.h"
#include <format>
#include <Core/source/infra/svc/Services.h>
#include <Core/source/infra/util/FolderResolver.h>

namespace p2c::infra::log
{
	namespace
	{
		std::mutex factoryMtx_;
		std::function<std::shared_ptr<Channel>()> defaultChannelFactory_;

		std::function<std::shared_ptr<Channel>()> GetDefaultChannelFactory_()
		{
			std::lock_guard lk{ factoryMtx_ };
			return defaultChannelFactory_;
		}

		std::shared_ptr<Channel>& GetDefaultChannel_(bool constructDefault)
		{
			static std::shared_ptr<Channel> pDefaultChannelSingleton;
			if (!pDefaultChannelSingleton && constructDefault) {
				if (auto factory = GetDefaultChannelFactory_()) {
					pDefaultChannelSingleton = factory();
				}
				else {
					std::wstring logFilePath;
					// try to use injected folder resolver to create logs stuff
					if (auto pResolver = svc::Services::ResolveOrNull<util::FolderResolver>())
					{
						logFilePath = pResolver->Resolve(util::FolderResolver::Folder::App, L"logs\\default.log");
					}
					else // default to a panic log in the temp folder
					{
						logFilePath = util::FolderResolver{}.Resolve(util::FolderResolver::Folder::Temp, L"p2c-panic.log");
					}
					auto pDefaultChannel = std::make_shared<Channel>();
					pDefaultChannel->AddDriver(std::make_unique<drv::SimpleFileDriver>(std::move(logFilePath)));
					pDefaultChannel->AddDriver(std::make_unique<drv::DebugOutDriver>());
					pDefaultChannelSingleton = std::move(pDefaultChannel);
				}
			}
			return pDefaultChannelSingleton;
		}
	}

	std::shared_ptr<Channel> GetDefaultChannel()
	{
		return GetDefaultChannel_(true);
	}

	void SetDefaultChannel(std::shared_ptr<Channel> pNewDefaultChannel)
	{
		GetDefaultChannel_(false) = std::move(pNewDefaultChannel);
	}

	void SetDefaultChannelFactory(std::function<std::shared_ptr<Channel>()> defaultChannelFactory)
	{
		std::lock_guard lk{ factoryMtx_ };
		defaultChannelFactory_ = std::move(defaultChannelFactory);
	}
}