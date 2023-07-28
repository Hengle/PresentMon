// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "WinAPI.h"
#include <Core/source/infra/log/Logging.h>


namespace p2c::win
{
    gfx::RectI GetWindowClientRect(HWND hWnd)
    {
        RECT cr;
        if (!GetClientRect(hWnd, &cr))
        {
            p2clog.warn().hr().commit();
            return { 0,0,0,0 };
        }
        POINT tl{ 0, 0 };
        if (!ClientToScreen(hWnd, &tl))
        {
            p2clog.warn().hr().commit();
            return { 0,0,0,0 };
        }
        return { tl.x, tl.y, tl.x + cr.right, tl.y + cr.bottom };
    }

    std::wstring GetWindowTitle(HWND hWnd)
    {
        wchar_t buffer[512];
        GetWindowTextW(hWnd, buffer, (int)std::size(buffer));
        return buffer;
    }
}