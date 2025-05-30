// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "SchemeHandlerFactory.h"
#include "SchemeFileHandler.h"
#include <include/cef_parser.h>
#include <format>
#include "util/Logging.h"


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
            pmlog_info(std::format("Processing request URL: {}", request->GetURL().ToString()));
            return nullptr;
        }
        else if (mode_ == SchemeMode::Local) {
            CefURLParts url_parts;
            if (!CefParseURL(request->GetURL(), url_parts)) {
                pmlog_error(std::format("Failed parsing URL: {}", request->GetURL().ToString())).no_trace();
                DoErrorMessage("URL Error", "Failed parsing URL, see log.");
            }
            else if (CefString(&url_parts.host) != localHost_ && CefString(&url_parts.port) != localPort_) {
                pmlog_error(std::format("URL does not match dev endpoint: {}", request->GetURL().ToString())).no_trace();
                DoErrorMessage("URL Error", "URL does not match dev endpoint, see log.");
            }
            else {
                pmlog_info(std::format("Processing request URL: {}", request->GetURL().ToString()));
            }
            return nullptr;
        }
        // otherwise mode is File (filesystem app assets) by default
        if (scheme_name == "https") {
            CefURLParts url_parts;
            if (!CefParseURL(request->GetURL(), url_parts)) {
                pmlog_error(std::format("Failed parsing URL: {}", request->GetURL().ToString())).no_trace();
                DoErrorMessage("URL Error", "Failed parsing URL, see log.");
                return nullptr;
            }
            else if (const auto host = CefString(&url_parts.host); host != "app") {
                pmlog_error(std::format("Non-app domain in File mode: {}", request->GetURL().ToString())).no_trace();
                DoErrorMessage("URL Error", "Non-app domain for File mode, see log.");
                return nullptr;
            }
            else {
                pmlog_dbg(std::format("Processing request URL: {}", request->GetURL().ToString()));
            }
            return new SchemeFileHandler(baseDir_);
        }
        // any other scheme in File mode is an error
        else {
            pmlog_error(std::format("Wrong scheme for File mode: {}", request->GetURL().ToString())).no_trace();
            DoErrorMessage("URL Error", "Wrong scheme for File mode, see log.");
            return new SchemeFileHandler(baseDir_);
        }
    }
}