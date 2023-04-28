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
		std::shared_ptr<Channel>& GetDefaultChannel_()
		{
			static std::shared_ptr<Channel> pDefaultChannel;
			if (!pDefaultChannel)
			{
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

				pDefaultChannel = std::make_shared<Channel>();
				pDefaultChannel->AddDriver(std::make_unique<drv::SimpleFileDriver>(std::move(logFilePath)));
				pDefaultChannel->AddDriver(std::make_unique<drv::DebugOutDriver>());
			}
			return pDefaultChannel;
		}
	}

	std::shared_ptr<Channel> GetDefaultChannel()
	{
		return GetDefaultChannel_();
	}

	void SetDefaultChannel(std::shared_ptr<Channel> pNewDefaultChannel)
	{
		GetDefaultChannel_() = std::move(pNewDefaultChannel);
	}
}