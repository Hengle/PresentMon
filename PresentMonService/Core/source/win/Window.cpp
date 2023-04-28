// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "Window.h"
#include <format>
#include <Core/source/infra/log/Logging.h>
#include <Core/source/infra/util/Util.h>
#include "MessageMap.h"
#include "WndClass.h"
#include <Core/source/infra/log/v/Window.h>

namespace p2c::win
{
    using namespace infra::log;

    Window::Window(std::wstring title, DWORD styles)
        :
        title{ std::move(title) }, 
        styles{ styles }
    {}

    Window::~Window()
    {
        p2cvlog(v::window).note(std::format(L"window dying hwn:[{:8x}] tit:[{}]", (uint64_t)hWnd, GetTitle())).commit();
        if (DestroyWindow(hWnd) == FALSE)
        {
            p2clog.hr().nox().commit();
        }
    }

    HWND Window::GetHandle()
    {
        return hWnd;
    }

    const std::wstring &Window::GetTitle() const
    {
        return title;
    }

    gfx::Vec2I Window::GetPosition() const
    {
        RECT rect{}; 
        if (GetWindowRect(hWnd, &rect) == FALSE)
        {
            p2clog.hr().warn(std::format(L"failed to get window rect {}", GetTitle())).commit();
        }
        return { rect.left, rect.top };
    }

    void Window::Move(gfx::Vec2I pos)
    {
        p2cvlog(v::window).note(std::format(L"pos:[{},{}]", pos.x, pos.y)).commit();
        if (SetWindowPos(hWnd, nullptr, pos.x, pos.y, 0, 0, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE) == FALSE)
        {
            p2clog.hr().warn(std::format(L"failed to move window {}", GetTitle())).commit();
        }
    }

    void Window::Reorder(HWND base)
    {
        p2cvlog(v::window).note(std::format(L"hwnd:{:8x}", (uint64_t)base)).commit();
        if (SetWindowPos(hWnd, GetNextWindow(base, GW_HWNDPREV), 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOMOVE) == FALSE)
        {
            p2clog.hr().warn(std::format(L"failed to reorder window {}", GetTitle())).commit();
        }
    }

    void Window::SetTopmost()
    {
        p2cvlog(v::window).commit();
        if (SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOMOVE) == FALSE)
        {
            p2clog.hr().warn(std::format(L"failed to make window topmost {}", GetTitle())).commit();
        }
    }

    void Window::ClearTopmost()
    {
        p2cvlog(v::window).commit();
        if (SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOMOVE) == FALSE)
        {
            p2clog.hr().warn(std::format(L"failed to make window non-topmost {}", GetTitle())).commit();
        }
    }

    void Window::Close()
    {
        p2cvlog(v::window).commit();
        if (PostMessage(hWnd, WM_CLOSE, 0, 0) == FALSE)
        {
            p2clog.hr().warn(std::format(L"failed to close window {}", GetTitle())).commit();
        }
    }

    LRESULT Window::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
    {
        LogMessage(msg, wParam, lParam);

        switch (msg)
        {
        case WM_SETTEXT:
            title = reinterpret_cast<wchar_t*>(lParam);
            break;
        }

        if (const auto result = CustomHandleMessage(msg, wParam, lParam))
        {
            return *result;
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    void Window::SetMessageLogging(bool enable)
    {
        loggingMessages = enable;
    }

    void Window::Hide()
    {
        p2cvlog(v::window).commit();
        ShowWindow(hWnd, SW_HIDE);
    }

    void Window::Show()
    {
        p2cvlog(v::window).commit();
        ShowWindow(hWnd, SW_SHOWNOACTIVATE);
    }

    void Window::Resize(gfx::DimensionsI clientSize)
    {
        p2cvlog(v::window).commit();
        const auto size = ComputeWindowDimensions(clientSize);
        if (SetWindowPos(hWnd, nullptr, 0, 0, size.width, size.height, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE) == FALSE)
        {
            p2clog.hr().warn(std::format(L"failed to resize window {}", GetTitle())).commit();
        }
    }

    gfx::DimensionsI Window::ComputeWindowDimensions(gfx::DimensionsI clientArea)
    {
        RECT wr{
            .left = 0,
            .top = 0,
            .right = clientArea.width,
            .bottom = clientArea.height,
        };
        if (AdjustWindowRect(&wr, styles, FALSE) == FALSE)
        {
            p2clog.hr().warn(L"Failed to adjust window rect").commit();
        }
        return RectToDims(wr);
    }

    ATOM Window::GetDefaultClass()
    {
        static WndClass defaultClass;
        return defaultClass.GetAtom();
    }

    std::optional<LRESULT> Window::CustomHandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
    {
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    void Window::SetHandle(HWND hWnd_)
    {
        p2cvlog(v::window).note(std::format(L"hwnd:{:8x}", (uint64_t)hWnd_)).commit();
        if (hWnd != nullptr)
        {
            p2clog.warn(L"handle already set for window").commit();
        }
        hWnd = hWnd_;
    }

    void Window::LogMessage(DWORD msg, LPARAM, WPARAM)
    {
        if (loggingMessages)
        {
            p2clog.info(std::format(L"WinMsg@[{}] : {}", GetTitle(), LookupMessageName(msg))).commit();
        }
    }

    const wchar_t *Window::LookupMessageName(DWORD msg)
    {
        static MessageMap messageMap;
        return messageMap.GetMessageName(msg);
    }

    gfx::DimensionsI RectToDims(const RECT& r)
    {
        return { r.right - r.left, r.bottom - r.top };
    }

    gfx::RectI RectToRectI(const RECT& r)
    {
        return { r.left, r.top, r.right, r.bottom };
    }
}
