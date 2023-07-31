// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include "../AsyncEndpoint.h"
#include "../AsyncEndpointManager.h"
#include <fstream>
#include "../CefValues.h"

namespace p2c::client::util::async
{
    class BrowseFolder : public AsyncEndpoint
    {
    public:
        static constexpr std::string GetKey() { return "browseFolder"; }
        BrowseFolder() : AsyncEndpoint{ AsyncEndpoint::Environment::BrowserProcess } {}
        // {} => {path: string}
        void ExecuteOnBrowser(uint64_t uid, CefRefPtr<CefValue> pArgObj, CefRefPtr<CefBrowser> pBrowser) const override
        {
            class BrowseFolderDialogCallback : public CefRunFileDialogCallback
            {
            public:
                explicit BrowseFolderDialogCallback(uint64_t uid, CefRefPtr<CefBrowser> pBrowser)
                    :
                    uid{ uid },
                    pBrowser{ std::move(pBrowser) }
                {}
                void OnFileDialogDismissed(const std::vector<CefString>& filePaths) override
                {
                    std::wstring path;

                    if (filePaths.size() > 0)
                    {
                        path = filePaths.front();
                    }

                    // 0:int uid, 1:bool success, 2:dict args
                    auto msg = CefProcessMessage::Create(AsyncEndpointManager::GetResolveMessageName());
                    {
                        const auto argList = msg->GetArgumentList();
                        argList->SetInt(0, (int)uid);
                        argList->SetBool(1, true); // success
                        argList->SetValue(2, MakeCefObject(CefProp{ "path", std::move(path) }));
                    }
                    pBrowser->GetMainFrame()->SendProcessMessage(PID_RENDERER, std::move(msg));
                }
            private:
                CefRefPtr<CefBrowser> pBrowser;
                uint64_t uid;
                IMPLEMENT_REFCOUNTING(BrowseFolderDialogCallback);
            };

            pBrowser->GetHost()->RunFileDialog(
                CefBrowserHost::FileDialogMode::FILE_DIALOG_OPEN_FOLDER, "Choose Capture Location",
                {}, {}, new BrowseFolderDialogCallback{ uid, pBrowser }
            );
        }
    };
}
