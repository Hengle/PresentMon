// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "AsyncEndpointManager.h"
#include <Core/source/infra/log/Logging.h>
#include <Core/source/infra/util/Util.h>
#include <algorithm>
#include "CefValues.h"
#include <include/cef_task.h>
#include "include/base/cef_callback.h"
#include "include/wrapper/cef_closure_task.h"
#include "CefValues.h"
#include <include/wrapper/cef_helpers.h>


namespace p2c::client::util
{
	void AsyncEndpointManager::DispatchInvocation(const std::string& key, CallbackContext ctx, CefRefPtr<CefV8Value> pObj, CefBrowser& browser, cef::DataBindAccessor& accessor, kern::Kernel& kernel)
	{
		CEF_REQUIRE_RENDERER_THREAD();

		// execute endpoint
		if (auto pEndpoint = endpoints.Find(key))
		{
			std::unique_lock lck{ mtx };
			const auto uid = nextUid++;
			auto&&[iInvoke, inserted] = invocations.emplace(std::piecewise_construct,
				std::forward_as_tuple(uid),
				std::forward_as_tuple(std::move(ctx), pEndpoint)
			);
			lck.unlock();

			switch (pEndpoint->GetEnvironment())
			{
			case AsyncEndpoint::Environment::BrowserProcess:
				{
					auto msg = CefProcessMessage::Create(GetDispatchMessageName());
					msg->GetArgumentList()->SetString(0, key);
					msg->GetArgumentList()->SetInt(1, (int)uid);
					msg->GetArgumentList()->SetDictionary(2, V8ToCefValue(*pObj)->GetDictionary());
					browser.GetMainFrame()->SendProcessMessage(PID_BROWSER, std::move(msg));
				}
				break;
			case AsyncEndpoint::Environment::RenderImmediate:
				pEndpoint->ExecuteOnRenderAccessor(uid, V8ToCefValue(*pObj), accessor);
				break;
			case AsyncEndpoint::Environment::KernelTask:
				iInvoke->second.taskFuture = std::async([&kernel, uid, pArgObj = V8ToCefValue(*pObj), pEndpoint, this]() mutable {
					AsyncEndpoint::Result result;
					try {
						// run the async command procudure on the std::async thread pool
						result = pEndpoint->ExecuteOnKernelTask(uid, std::move(pArgObj), kernel);
					} catch (...) {
						result = { false, CefValueNull() };
					}
					// pass results and run the resolve logic (V8 conversion etc.) on the renderer thread
					CefPostTask(TID_RENDERER, base::BindOnce(
						&AsyncEndpointManager::ResolveInvocation, base::Unretained(this),
						uid, result.succeeded, std::move(result.pArgs)
					));
				});
				break;
			}
		}
		else
		{
			p2clog.note(std::format(L"Key [{}] does not have an async endpoint registered", infra::util::ToWide(key)))
				.nox().notrace().commit();
		}
	}

	void AsyncEndpointManager::ResolveInvocation(uint64_t uid, bool success, CefRefPtr<CefValue> pArgs)
	{
		CEF_REQUIRE_RENDERER_THREAD();

		std::unique_lock lck{ mtx };
		if (auto i = invocations.find(uid); i != std::end(invocations))
		{
			auto invocation = std::move(i->second);
			invocations.erase(i);
			lck.unlock();

			auto& ctx = invocation.context;
			ctx.pV8Context->Enter();

			auto pArgsV8 = util::CefToV8Value(*pArgs);
			if (success)
			{
				ctx.pV8ResolveFunction->ExecuteFunction(nullptr, { std::move(pArgsV8) });
			}
			else
			{
				ctx.pV8RejectFunction->ExecuteFunction(nullptr, { std::move(pArgsV8) });
			}

			ctx.pV8Context->Exit();
		}
		else
		{
			lck.unlock();
			p2clog.note(L"Failed to resolve async invocation (not registered).").nox().notrace().commit();
		}
	}
}