// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "EventHookManager.h"

namespace p2c::win
{
    EventHookManager::Token EventHookManager::AddHandler(std::shared_ptr<EventHookHandler> pHandler)
    {
        return { Get().AddHandlerInternal(pHandler) };
    }
    HWINEVENTHOOK EventHookManager::AddHandlerInternal(std::shared_ptr<EventHookHandler> pHandler)
    {
        const auto filter = pHandler->GetFilter();
        const auto hHook = SetWinEventHook(
            filter.minEvent, filter.maxEvent, nullptr,
            &RouteWinEvent, filter.pid, filter.tid, WINEVENT_OUTOFCONTEXT
        );
        if (!hHook)
        {
            p2clog.note(std::format(L"Failure hooking process {}", filter.pid)).hr().commit();
        }
        handlerMap.emplace(hHook, pHandler);
        return hHook;
    }

    void EventHookManager::RouteWinEvent(HWINEVENTHOOK hook, DWORD event, HWND hWnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime)
    {
        if (auto i = Get().handlerMap.find(hook); i != Get().handlerMap.end())
        {
            i->second->Handle(hook, event, hWnd, idObject, idChild, dwEventThread, dwmsEventTime);
        }
        else
        {
            p2clog.warn(L"Received unhandled win hook callback invocation").commit();
        }
    }

    EventHookManager& EventHookManager::Get()
    {
        static EventHookManager man;
        return man;
    }

    void EventHookManager::RemoveHandler(HWINEVENTHOOK hHook)
    {
        if (auto i = handlerMap.find(hHook); i != handlerMap.end())
        {
            if (UnhookWinEvent(i->first))
            {
                handlerMap.erase(i);
                return;
            }
        }
        p2clog.warn(L"Failure unhooking windows event listener").commit();
    }

    EventHookManager::~EventHookManager()
    {
        for (auto& item : handlerMap)
        {
            UnhookWinEvent(item.first);
        }
    }


    // token section

    EventHookManager::Token::Token(HWINEVENTHOOK hHook) : hHook{ hHook } {}
    EventHookManager::Token::Token(Token&& donor) noexcept
    {
        *this = std::move(donor);
    }
    EventHookManager::Token& EventHookManager::Token::operator=(Token&& donor) noexcept
    {
        if (&donor != this) {
            hHook = donor.hHook;
            donor.hHook = nullptr;
        }
        return *this;
    }
    EventHookManager::Token::~Token()
    {
        if (hHook) {
            try { EventHookManager::Get().RemoveHandler(hHook); }
            catch (...) {}
        }
    }
}