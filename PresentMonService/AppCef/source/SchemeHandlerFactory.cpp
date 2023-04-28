// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "SchemeHandlerFactory.h"
#include "SchemeFileHandler.h"
#include <include/cef_parser.h>
#include <format>
#include <Core/source/infra/log/Logging.h>


namespace p2c::client::cef
{
    SchemeHandlerFactory::SchemeHandlerFactory()
    {
        baseDir = std::filesystem::current_path() / "Web\\";
    }

    // Return a new scheme handler instance to handle the request.
    CefRefPtr<CefResourceHandler> SchemeHandlerFactory::Create(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        const CefString& scheme_name,
        CefRefPtr<CefRequest> request)
    {
        if (scheme_name == "https")
        {
            CefURLParts url_parts;
            if (!CefParseURL(request->GetURL(), url_parts))
            {
                p2clog.note(std::format(L"Failed parsing URL: {}", request->GetURL().ToWString())).nox().commit();
                return nullptr;
            }
            if (const auto host = CefString(&url_parts.host); host == "app")
            {
                return new SchemeFileHandler(baseDir);
            }
        }
        return nullptr;
    }
}