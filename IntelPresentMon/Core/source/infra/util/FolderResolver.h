// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <string>

namespace p2c::infra::util
{
	class FolderResolver
	{
	public:
		// types
		enum class Folder
		{
			App,
			Temp,
			Captures,
			Install,
		};
		// functions
		FolderResolver(std::wstring appFolderName = {}, bool createSubdirectories = true);
		std::wstring Resolve(Folder f, std::wstring path = {}) const;
	private:
		std::wstring appPath;
	};
}