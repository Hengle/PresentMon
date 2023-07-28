// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "SchemeHandlerFactory.h"
#include "SchemeFileHandler.h"
#include <include/cef_parser.h>
#include <format>
#include <Core/source/infra/log/Logging.h>


namespace p2c::client::cef
{
    SchemeHandlerFactory::SchemeHandlerFactory(SchemeMode mode, bool hardFail, std::string localHost, std::string localPort)
        :
        baseDir_{ std::filesystem::current_path() / "Web\\" },
        mode_{ mode },
        hardFail_{ hardFail },
        localHost_{ std::move(localHost) },
        localPort_{ std::move(localPort) }
    {}

    // Return a new scheme handler instance to handle the request.
    CefRefPtr<CefResourceHandler> SchemeHandlerFactory::Create(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        const CefString& scheme_name,
        CefRefPtr<CefRequest> request)
    {
        const auto DoErrorMessage = [&](std::string title, std::string body) {
            if (hardFail_) {
                MessageBoxA(
                    browser->GetHost()->GetWindowHandle(),
                    body.c_str(), title.c_str(),
                    MB_ICONERROR | MB_APPLMODAL
                );
            }
        };

        if (mode_ == SchemeMode::Web) {
            // anything goes if web mode
            // but don't worry about loading app files (only use default schema managers)
            p2clog.info(std::format(L"Processing request URL: {}", request->GetURL().ToWString())).commit();
            return nullptr;
        }
        else if (mode_ == SchemeMode::Local) {
            CefURLParts url_parts;
            if (!CefParseURL(request->GetURL(), url_parts)) {
                p2clog.note(std::format(L"Failed parsing URL: {}", request->GetURL().ToWString())).notrace().nox().commit();
                DoErrorMessage("URL Error", "Failed parsing URL, see log.");
            }
            else if (CefString(&url_parts.host) != localHost_ && CefString(&url_parts.port) != localPort_) {
                p2clog.note(std::format(L"URL does not match dev endpoint: {}", request->GetURL().ToWString())).notrace().nox().commit();
                DoErrorMessage("URL Error", "URL does not match dev endpoint, see log.");
            }
            else {
                p2clog.info(std::format(L"Processing request URL: {}", request->GetURL().ToWString())).commit();
            }
            return nullptr;
        }
        // otherwise mode is File (filesystem app assets) by default
        if (scheme_name == "https") {
            CefURLParts url_parts;
            if (!CefParseURL(request->GetURL(), url_parts)) {
                p2clog.note(std::format(L"Failed parsing URL: {}", request->GetURL().ToWString())).notrace().nox().commit();
                DoErrorMessage("URL Error", "Failed parsing URL, see log.");
                return nullptr;
            }
            else if (const auto host = CefString(&url_parts.host); host != "app") {
                p2clog.note(std::format(L"Non-app domain in File mode: {}", request->GetURL().ToWString())).notrace().nox().commit();
                DoErrorMessage("URL Error", "Non-app domain for File mode, see log.");
                return nullptr;
            }
            else {
                p2clog.info(std::format(L"Processing request URL: {}", request->GetURL().ToWString())).commit();
            }
            return new SchemeFileHandler(baseDir_);
        }
        // any other scheme in File mode is an error
        else {
            p2clog.note(std::format(L"Wrong scheme for File mode: {}", request->GetURL().ToWString())).notrace().nox().commit();
            DoErrorMessage("URL Error", "Wrong scheme for File mode, see log.");
            return new SchemeFileHandler(baseDir_);
        }
    }
}