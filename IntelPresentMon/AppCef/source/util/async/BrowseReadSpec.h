// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include "../AsyncEndpoint.h"
#include "../AsyncEndpointManager.h"
#include <fstream>
#include "../CefValues.h"

namespace p2c::client::util::async
{
	class BrowseReadSpec : public AsyncEndpoint
	{
	public:
        static constexpr std::string GetKey() { return "browseReadSpec"; }
		BrowseReadSpec() : AsyncEndpoint{ AsyncEndpoint::Environment::BrowserProcess } {}
        // {} => {payload: string}
		void ExecuteOnBrowser(uint64_t uid, CefRefPtr<CefValue> pArgObj, CefRefPtr<CefBrowser> pBrowser) const override
		{
            class ReadSpecDialogCallback : public CefRunFileDialogCallback
            {
            public:
                explicit ReadSpecDialogCallback(uint64_t uid, CefRefPtr<CefBrowser> pBrowser)
                    :
                    uid{ uid },
                    pBrowser{ std::move(pBrowser) }
                {}
                void OnFileDialogDismissed(const std::vector<CefString>& filePaths) override
                {
                    std::wstring payload;

                    if (filePaths.size() > 0)
                    {
                        std::wifstream file{ filePaths[0].ToString() };
                        payload = std::wstring(
                            std::istreambuf_iterator<wchar_t>{ file },
                            std::istreambuf_iterator<wchar_t>{}
                        );
                    }

                    // 0:int uid, 1:bool success, 2:dict args
                    auto msg = CefProcessMessage::Create(AsyncEndpointManager::GetResolveMessageName());
                    {
                        const auto argList = msg->GetArgumentList();
                        argList->SetInt(0, (int)uid);
                        argList->SetBool(1, true); // success
                        argList->SetValue(2, MakeCefObject(CefProp{ "payload", std::move(payload) }));
                    }
                    pBrowser->GetMainFrame()->SendProcessMessage(PID_RENDERER, std::move(msg));
                }
            private:
                CefRefPtr<CefBrowser> pBrowser;
                uint64_t uid;
                IMPLEMENT_REFCOUNTING(ReadSpecDialogCallback);
            };

            pBrowser->GetHost()->RunFileDialog(
                CefBrowserHost::FileDialogMode::FILE_DIALOG_OPEN, "Load Configuration",
                {}, { ".json" }, new ReadSpecDialogCallback{ uid, pBrowser }
            );
		}
	};
}