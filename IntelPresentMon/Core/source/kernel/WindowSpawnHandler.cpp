// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "WindowSpawnHandler.h"
#include "Overlay.h"
#include "OverlayContainer.h"
#include <Core/source/infra/log/v/ProcWatch.h>

using p2c::infra::log::v::procwatch;

namespace p2c::kern
{
    WindowSpawnHandler::WindowSpawnHandler(DWORD pid, OverlayContainer* pOverlay) : pid{ pid }, pOverlay{ pOverlay }
    {
        p2cvlog(procwatch).note(std::format(L"win spawn handler ctor | pid:{:5x}", pid)).commit();
    }

    win::EventHookHandler::Filter WindowSpawnHandler::GetFilter() const
    {
        return {
            .minEvent = EVENT_OBJECT_CREATE,
            .maxEvent = EVENT_OBJECT_CREATE,
            .pid = pid,
        };
    }

    void WindowSpawnHandler::Handle(
        HWINEVENTHOOK hook, DWORD event, HWND hWnd,
        LONG idObject, LONG idChild,
        DWORD dwEventThread, DWORD dwmsEventTime)
    {
        if constexpr (procwatch) {
            RECT r{};
            GetWindowRect(hWnd, &r);
            p2clog.verbose(std::format(L"win-spawn-event | pid:{:5} hwd:{:8x} own:{:8x} vis:{} siz:{} nam:{}",
                pid,
                reinterpret_cast<uintptr_t>(hWnd),
                reinterpret_cast<uintptr_t>(GetWindow(hWnd, GW_OWNER)),
                IsWindowVisible(hWnd),
                win::RectToDims(r).GetArea(),
                win::GetWindowTitle(hWnd)
            )).commit();
        }

        // filter to only windows without an owner window
        // window could have died between creation and when we get around to handling
        // so check handle validity first
        if (idObject == OBJID_WINDOW && IsWindow(hWnd) && GetWindow(hWnd, GW_OWNER) == nullptr) {
            if (RECT r;  GetWindowRect(hWnd, &r)) {
                // filter to window with dimensions
                if (r.right - r.left > 0) {
                    pOverlay->RegisterWindowSpawn(pid, hWnd, r);
                }
            }
        }
    }
}