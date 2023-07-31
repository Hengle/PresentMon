// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "CefIpcLogDriver.h"
#include <include/cef_app.h>
#include "CefIpcLogRouter.h"
#include <include/cef_task.h>
#include "include/base/cef_callback.h"
#include "include/wrapper/cef_closure_task.h"

namespace p2c::client::util::log
{
	CefIpcLogDriver::CefIpcLogDriver(CefRefPtr<CefBrowser> pBrowser)
		:
		pBrowser{ std::move(pBrowser) }
	{}

	void CefIpcLogDriver::Commit(const infra::log::EntryOutputBase& entry)
	{
		if (CefCurrentlyOn(TID_RENDERER))
		{
			DoIpcSend(entry);
		}
		else
		{
			CefPostTask(TID_RENDERER, base::BindOnce(&CefIpcLogDriver::DoIpcSend, base::Unretained(this), entry));
		}
	}

	void CefIpcLogDriver::DoIpcSend(const infra::log::EntryOutputBase& entry)
	{
		auto msg = CefProcessMessage::Create(CefIpcLogRouter::ipcChannelName);
		msg->GetArgumentList()->SetString(0, FormatEntry(entry));
		pBrowser->GetMainFrame()->SendProcessMessage(PID_BROWSER, std::move(msg));
	}
}