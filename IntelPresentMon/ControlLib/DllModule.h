// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#define NOMINMAX
#include <Windows.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <format>

class DllModule
{
public:
    DllModule(const DllModule& t) = delete;
    DllModule& operator=(const DllModule& t) = delete;
	DllModule(const std::vector<std::string>& dllFiles)
	{
		if (dllFiles.empty())
		{
			throw std::runtime_error{ "No dll files specified for dll module" };
		}
		// try list of dll candidates in order
		for (const auto& file : dllFiles)
		{
			if (hModule = LoadLibraryExA(file.c_str(), NULL, LOAD_LIBRARY_SEARCH_SYSTEM32))
			{
				return;
			}
		}
		// if all failed to load, throw exception
		throw std::runtime_error{ std::format("Failed to load library, last DLL tried: {}", dllFiles.back().c_str()) };
	}
	~DllModule()
	{
		FreeLibrary(hModule);
	}
	void* GetProcAddress(const std::string& procName) const
	{
		return ::GetProcAddress(hModule, procName.c_str());
	}
private:
	HMODULE hModule = nullptr;
};