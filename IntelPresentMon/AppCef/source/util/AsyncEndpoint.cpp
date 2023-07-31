// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "AsyncEndpoint.h"
#include <Core/source/infra/log/Logging.h>

namespace p2c::client::util
{
	AsyncEndpoint::AsyncEndpoint(Environment env) : env{ env } {}

	void AsyncEndpoint::ExecuteOnBrowser(uint64_t uid, CefRefPtr<CefValue> pArgObj, CefRefPtr<CefBrowser> pBrowser) const
	{
		p2clog.warn(L"virtual not implemented").commit();
	}

	void AsyncEndpoint::ExecuteOnRenderAccessor(uint64_t uid, CefRefPtr<CefValue> pArgObj, cef::DataBindAccessor& accessor) const
	{
		p2clog.warn(L"virtual not implemented").commit();
	}

	AsyncEndpoint::Result AsyncEndpoint::ExecuteOnKernelTask(uint64_t uid, CefRefPtr<CefValue> pArgObj, kern::Kernel& kernel) const
	{
		p2clog.warn(L"virtual not implemented").commit();
		return {};
	}

	AsyncEndpoint::Environment AsyncEndpoint::GetEnvironment() const { return env; }
}