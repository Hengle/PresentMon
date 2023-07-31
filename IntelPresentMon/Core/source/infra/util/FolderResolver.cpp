// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "FolderResolver.h"
#include <Core/source/win/WinAPI.h>
#include <ShlObj.h>
#include <filesystem>
#include <format>
#include <Core/source/infra/log/Logging.h>

namespace p2c::infra::util
{
	FolderResolver::FolderResolver(std::wstring appFolderName, bool createSubdirectories)
	{
		if (!appFolderName.empty())
		{
			wchar_t* pPath = nullptr;
			if (auto hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &pPath); FAILED(hr))
			{
				CoTaskMemFree(pPath);
				pPath = nullptr;
				p2clog.note(L"Failed getting local app data path").commit();
			}
			const auto dir = std::format(L"{}\\{}", pPath, appFolderName);
			try {
				std::filesystem::create_directories(dir);
				appPath = dir;
			}
			catch (const std::exception& e) {
				CoTaskMemFree(pPath);
				pPath = nullptr;
				p2clog.note(L"Failed creating directory: " + dir).nested(e).commit();
			}
			if (pPath)
			{
				CoTaskMemFree(pPath);
			}
		}
		else
		{
			appPath = std::filesystem::current_path().wstring();
		}

		if (createSubdirectories) {
			// captures folder 
			try {
				std::filesystem::create_directory(std::format(L"{}\\Captures", appPath));
			}
			catch (const std::exception& e) {
				p2clog.note(L"Failed creating directory: " + std::format(L"{}\\Captures", appPath)).nested(e).commit();
			}
			// configs folder
			try {
				std::filesystem::create_directory(std::format(L"{}\\Configs", appPath));
			}
			catch (const std::exception& e) {
				p2clog.note(L"Failed creating directory: " + std::format(L"{}\\Configs", appPath)).nested(e).commit();
			}
		}
	}

	std::wstring FolderResolver::Resolve(Folder f, std::wstring path) const
	{
		switch (f)
		{
		case Folder::App:
			if (appPath.empty())
			{
				p2clog.note(L"Failed to resolve app path: not initialized").commit();
				return {};
			}
			if (path.empty())
			{
				return appPath;
			}
			else
			{
				return std::format(L"{}\\{}", appPath, path);
			}
		case Folder::Temp:
		{
			wchar_t tempPath[MAX_PATH + 1];
			if (!GetTempPathW(MAX_PATH, tempPath))
			{
				p2clog.note(L"failed resolving temp dir").hr().commit();
			}
			return std::format(L"{}{}", tempPath, path);
		}
		case Folder::Captures:
		{
			if (appPath.empty())
			{
				p2clog.note(L"Failed to resolve Captures path: appPath not initialized").commit();
				return {};
			}
			if (!path.empty())
			{
				p2clog.warn(L"Resoved Captures folder with path string").commit();
			}
			return std::format(L"{}\\Captures", appPath);
		}
		case Folder::Install:
		{
			wchar_t modulePath[MAX_PATH];
			::GetModuleFileNameW(nullptr, modulePath, (DWORD)std::size(modulePath));
			const auto dir = std::filesystem::path{ modulePath }.remove_filename().wstring();
			return std::format(L"{}{}", dir, path);
		}
		}
		return {};
	}
}