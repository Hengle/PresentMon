// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <Core/source/win/WinAPI.h>
#include <Core/source/infra/log/Driver.h>
#include <include/internal/cef_ptr.h>

class CefBrowser;

namespace p2c::client::util::log
{
	class CefIpcLogDriver : public infra::log::Driver
	{
	public:
		CefIpcLogDriver(CefRefPtr<CefBrowser> pBrowser);
		void Commit(const infra::log::EntryOutputBase&) override;
	private:
		void DoIpcSend(const infra::log::EntryOutputBase& entry);
		CefRefPtr<CefBrowser> pBrowser;
	};
}