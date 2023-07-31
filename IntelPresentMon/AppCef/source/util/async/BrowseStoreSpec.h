// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include "../AsyncEndpoint.h"
#include "../AsyncEndpointManager.h"
#include <fstream>

namespace p2c::client::util::async
{
	class BrowseStoreSpec : public AsyncEndpoint
	{
	public:
        static constexpr std::string GetKey() { return "browseStoreSpec"; }
        BrowseStoreSpec() : AsyncEndpoint{ AsyncEndpoint::Environment::BrowserProcess } {}
        // {payload: string} => {written: bool}
		void ExecuteOnBrowser(uint64_t uid, CefRefPtr<CefValue> pArgObj, CefRefPtr<CefBrowser> pBrowser) const override
		{
            class BrowseStoreSpecDialogCallback : public CefRunFileDialogCallback
            {
            public:
                explicit BrowseStoreSpecDialogCallback(CefRefPtr<CefBrowser> pBrowser, uint64_t uid, std::wstring payload)
                    :
                    pBrowser{ std::move(pBrowser) },
                    uid{ uid },
                    payload{ std::move(payload) }
                {}
                void OnFileDialogDismissed(const std::vector<CefString>& filePaths) override
                {
                    bool written = false;
                    bool failed = false;

                    if (filePaths.size() > 0)
                    {
                        std::wofstream file{ filePaths[0].ToString() };
                        file << payload;
                        failed = !file.good();
                        written = true;
                    }

                    // 0:int uid, 1:bool success, 2:dict args
                    auto msg = CefProcessMessage::Create(AsyncEndpointManager::GetResolveMessageName());
                    msg->GetArgumentList()->SetInt(0, (int)uid);
                    if (!failed)
                    {
                        msg->GetArgumentList()->SetBool(1, true); // success
                        msg->GetArgumentList()->SetValue(2, MakeCefObject(CefProp{ "written", written }));
                    }
                    else
                    {
                        msg->GetArgumentList()->SetBool(1, false); // failure
                        msg->GetArgumentList()->SetDictionary(2, std::move(CefDictionaryValue::Create()));
                    }
                    pBrowser->GetMainFrame()->SendProcessMessage(PID_RENDERER, std::move(msg));
                }
            private:
                CefRefPtr<CefBrowser> pBrowser;
                uint64_t uid;
                std::wstring payload;
                IMPLEMENT_REFCOUNTING(BrowseStoreSpecDialogCallback);
            };

            pBrowser->GetHost()->RunFileDialog(
                CefBrowserHost::FileDialogMode::FILE_DIALOG_SAVE, "Save Configuration", {}, { ".json" },
                new BrowseStoreSpecDialogCallback{ pBrowser, uid, pArgObj->GetDictionary()->GetString("payload").ToWString() }
            );
		}
	};
}