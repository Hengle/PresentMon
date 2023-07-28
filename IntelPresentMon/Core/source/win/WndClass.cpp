// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "WndClass.h"
#include "Window.h"
#include <Core/source/infra/log/Logging.h>

namespace p2c::win
{
    WndClass::WndClass()
    {
        WNDCLASS wc = {};
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"PMON2-CAP-CLS";
        wc.style = 0;
        wc.lpfnWndProc = &SetupWndProc;
        atom = RegisterClassW(&wc);
        if (atom == 0)
        {
            p2clog.hr().commit();
        }
    }

    WndClass::~WndClass()
    {
        if (UnregisterClassW(MAKEINTATOM(atom), nullptr) == FALSE)
        {
            p2clog.hr().nox().commit();
        }
    }

    ATOM WndClass::GetAtom() const
    {
        return atom;
    }

    LRESULT WndClass::SetupWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        // use create parameter passed in from CreateWindow() to store window class pointer at WinAPI side
        if (msg == WM_NCCREATE)
        {
            // extract ptr to window class instance from creation data
            const CREATESTRUCTW* const pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
            auto const pWnd = static_cast<Window*>(pCreate->lpCreateParams);
            // Set handle into instance
            pWnd->SetHandle(hWnd);
            // set WinAPI-managed user data to store ptr to window instance
            {
                SetLastError(0);
                const auto ret = SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWnd));
                if (ret == 0)
                {
                    if (const auto hr = GetLastError(); FAILED(hr))
                    {
                        p2clog.hr(hr).commit();
                    }                    
                }
            }
            // set message proc to normal (non-setup) handler now that setup is finished            
            {
                SetLastError(0);
                const auto ret = SetWindowLongPtrW(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&ThunkWndProc));
                if (ret == 0)
                {
                    if (const auto hr = GetLastError(); FAILED(hr))
                    {
                        p2clog.hr(hr).commit();
                    }
                }
            }
            // forward message to window instance handler
            return pWnd->HandleMessage(msg, wParam, lParam);
        }
        // if we get a message before the WM_NCCREATE message, handle with default handler
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    LRESULT WndClass::ThunkWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        // retrieve ptr to window instance
        auto const pWnd = reinterpret_cast<Window*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
        // forward message to window instance handler
        return pWnd->HandleMessage(msg, wParam, lParam);
    }
}
