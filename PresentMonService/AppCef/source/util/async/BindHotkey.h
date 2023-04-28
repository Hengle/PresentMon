// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include "../AsyncEndpoint.h"
#include <include/cef_task.h>
#include "include/base/cef_callback.h"
#include "include/wrapper/cef_closure_task.h"
#include "../../DataBindAccessor.h"
#include <Core/source/infra/log/Logging.h>
#include "../CefValues.h"

namespace p2c::client::util::async
{
	class BindHotkey : public AsyncEndpoint
	{
	public:
        static constexpr std::string GetKey() { return "bindHotkey"; }
        BindHotkey() : AsyncEndpoint{ AsyncEndpoint::Environment::RenderImmediate } {}
        // {combination: {modifiers:[], hotkey:int}, action:int} => null
		void ExecuteOnRenderAccessor(uint64_t uid, CefRefPtr<CefValue> pArgObj, cef::DataBindAccessor& accessor) const override
		{
            accessor.BindHotkey(*pArgObj, [uid = uid, pAccessor = &accessor](bool succeeded) {
                CefPostTask(TID_RENDERER, base::BindOnce(Resolve_, uid, succeeded, CefRefPtr<cef::DataBindAccessor>{ pAccessor }));
            });
		}
    private:
        static void Resolve_(uint64_t uid, bool succeeded, CefRefPtr<cef::DataBindAccessor> pAccessor)
        {
            pAccessor->ResolveAsyncEndpoint(uid, succeeded, CefValueNull());
        }
	};
}