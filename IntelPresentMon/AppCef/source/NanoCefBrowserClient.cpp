// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "NanoCefBrowserClient.h"
#include <include/wrapper/cef_helpers.h>
#include "util/CefIpcLogRouter.h"
#include <Core/source/infra/log/Logging.h>
#include <format>
#include <fstream>
#include <streambuf>
#include "util/AsyncEndpointManager.h"
#include "NanoCefProcessHandler.h"

// implemented in winmain.cpp
void AppQuitMessageLoop();

namespace p2c::client::cef
{
    NanoCefBrowserClient::NanoCefBrowserClient()
    {
        class NullMenuHandler : public CefContextMenuHandler {
        public:
            NullMenuHandler() = default;
            void OnBeforeContextMenu(CefRefPtr<CefBrowser> browser,
                CefRefPtr<CefFrame> frame,
                CefRefPtr<CefContextMenuParams> params,
                CefRefPtr<CefMenuModel> model) override {
                // Clear the context menu to disable the default right-click behavior.
                model->Clear();
            }

            IMPLEMENT_REFCOUNTING(NullMenuHandler);
            DISALLOW_COPY_AND_ASSIGN(NullMenuHandler);
        };
        pContextMenuHandler = new NullMenuHandler{};
    }

    CefRefPtr<CefBrowser> NanoCefBrowserClient::GetBrowser()
    {
        return pBrowser;
    }

    CefRefPtr<CefLifeSpanHandler> NanoCefBrowserClient::GetLifeSpanHandler()
    {
        return this;
    }

    void NanoCefBrowserClient::OnAfterCreated(CefRefPtr<CefBrowser> browser_)
    {
        pBrowser = browser_;

        CefLifeSpanHandler::OnAfterCreated(std::move(browser_));
    }

    void NanoCefBrowserClient::OnBeforeClose(CefRefPtr<CefBrowser> browser_)
    {
        pBrowser.reset();
        AppQuitMessageLoop();

        CefLifeSpanHandler::OnBeforeClose(browser_);
    }

    bool NanoCefBrowserClient::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId source_process, CefRefPtr<CefProcessMessage> message)
    {
        // Handle log messages from other processes
        if (message->GetName() == util::log::CefIpcLogRouter::ipcChannelName)
        {
            util::log::CefIpcLogRouter::Route(
                message->GetArgumentList()->GetString(0).ToWString()
            );
            return true;
        }
        // 0: endpoint id (name), 1: uid, 2: args (obj/dict)
        else if (message->GetName() == util::AsyncEndpointManager::GetDispatchMessageName())
        {
            if (auto pEndpoint = endpoints.Find(message->GetArgumentList()->GetString(0)))
            {
                pEndpoint->ExecuteOnBrowser(message->GetArgumentList()->GetInt(1), message->GetArgumentList()->GetValue(2), browser);
            }
            else
            {
                p2clog.warn(std::format(L"Endpoint async not found: {}", message->GetArgumentList()->GetString(0).ToWString())).commit();
            }
            return true;
        }
        else if (message->GetName() == NanoCefProcessHandler::GetShutdownMessageName())
        {
            shutdownAcknowledgementFlag = true;
            // this causes another WM_CLOSE
            pBrowser->GetHost()->CloseBrowser(true);
            return true;
        }
        return false;
    }

    std::optional<LRESULT> NanoCefBrowserClient::HandleCloseMessage()
    {
        // if a previous close was sent and acknowledged to/from render process (or timed out), proceed with close
        if (shutdownAcknowledgementFlag && shutdownSemaphore.try_acquire()) {
            return {};
        }
        // shutdown was requested, but not acked yet (or timeout fired already)
        if (shutdownRequestFlag) {
            return 0;
        }
        // if this is the first close request, don't close and instead send request to render process
        else {
            shutdownRequestFlag = true;
            // send message to render process to destroy the kernel and kernel-associated objects
            auto msg = CefProcessMessage::Create(NanoCefProcessHandler::GetShutdownMessageName());
            pBrowser->GetMainFrame()->SendProcessMessage(PID_RENDERER, std::move(msg));
            // set a timeout to force close if ack is not received from render process
            std::thread{ [this] {
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(1000ms);
                shutdownAcknowledgementFlag = true;
                pBrowser->GetHost()->CloseBrowser(true);
                p2clog.warn(L"Shutdown ack timed out").commit();
            } }.detach();
            return 0;
        }
    }

    CefRefPtr<CefContextMenuHandler> NanoCefBrowserClient::GetContextMenuHandler()
    {
        return pContextMenuHandler;
    }
}