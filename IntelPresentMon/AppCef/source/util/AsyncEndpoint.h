// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <Core/source/win/WinAPI.h>
#include <include/cef_v8.h>
#include <string>
#include <functional>

namespace p2c::client::cef
{
	class DataBindAccessor;
}

namespace p2c::kern
{
	class Kernel;
}

namespace p2c::client::util
{
	class AsyncEndpointManager;

	class AsyncEndpoint
	{
	public:
		// types
		enum class Environment
		{
			BrowserProcess,
			RenderImmediate,
			KernelTask,
		};
		struct Result
		{
			bool succeeded = false;
			CefRefPtr<CefValue> pArgs;
		};
		// functions
		AsyncEndpoint(Environment env);
		virtual void ExecuteOnBrowser(uint64_t uid, CefRefPtr<CefValue> pArgObj, CefRefPtr<CefBrowser> pBrowser) const;
		virtual void ExecuteOnRenderAccessor(uint64_t uid, CefRefPtr<CefValue> pArgObj, cef::DataBindAccessor& accessor) const;
		virtual Result ExecuteOnKernelTask(uint64_t uid, CefRefPtr<CefValue> pArgObj, kern::Kernel& kernel) const;
		Environment GetEnvironment() const;
	private:
		// data
		Environment env;
	};
}