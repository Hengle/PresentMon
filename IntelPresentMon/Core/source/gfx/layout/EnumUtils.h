// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <Core/source/infra/log/Logging.h>

namespace p2c::gfx::lay
{
	template<typename> struct EnumRegistry;
}


#define XEN_KEY_TEM(key) key,

#define XEN_GENERATE_ENUM(name, list) \
enum class name \
{ \
	list(XEN_KEY_TEM) \
};


#define XEN_PARSE_TEM(key) if (s == L ## #key) return key;
#define XEN_STRING_TEM(key) case key: return L ## #key;

#define XEN_GENERATE_REGISTRY(name, list) \
template<> struct EnumRegistry<name> \
{ \
	static name ToEnum(const std::wstring& s) \
	{ \
		using enum name; \
		list(XEN_PARSE_TEM) \
		p2clog.note(L"cannot convert to enum type: " L ## #name).commit(); \
		return name(0); \
	} \
	static std::wstring FromEnum(name e) \
	{ \
		using enum name; \
		switch (e) \
		{ \
		list(XEN_STRING_TEM) \
		} \
		p2clog.note(L"cannot convert from enum type: " L ## #name).commit(); \
		return L""; \
	} \
};