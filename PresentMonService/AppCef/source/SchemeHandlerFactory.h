// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <Core/source/win/WinAPI.h>
#include <include/cef_scheme.h>
#include <include/wrapper/cef_helpers.h>
#include <filesystem>
#include <fstream>


namespace p2c::client::cef
{
    class SchemeHandlerFactory : public CefSchemeHandlerFactory
    {
    public:
        SchemeHandlerFactory();

        // Returns a new scheme handler instance to handle the request.
        CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser,
            CefRefPtr<CefFrame> frame,
            const CefString& scheme_name,
            CefRefPtr<CefRequest> request) override;
    private:
        std::filesystem::path baseDir;

        IMPLEMENT_REFCOUNTING(SchemeHandlerFactory);
        DISALLOW_COPY_AND_ASSIGN(SchemeHandlerFactory);
    };
}