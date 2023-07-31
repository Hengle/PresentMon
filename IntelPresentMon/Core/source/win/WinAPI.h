// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#define _WIN32_WINNT 0x0602
#include <sdkddkver.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define STRICT
#include <windows.h>
// remove annoying A/W macros as necessary
#undef FormatMessage
#include <Core/source/gfx/base/Geometry.h>


namespace p2c::win
{
    gfx::RectI GetWindowClientRect(HWND hWnd);
    std::wstring GetWindowTitle(HWND hWnd);
}