// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include "../AsyncEndpoint.h"
#include "../../DataBindAccessor.h"
#include <Core/source/infra/log/Logging.h>
#include "../CefValues.h"

namespace p2c::client::util::async
{
    class LaunchKernel : public AsyncEndpoint
    {
    public:
        static constexpr std::string GetKey() { return "launchKernel"; }
        LaunchKernel() : AsyncEndpoint{ AsyncEndpoint::Environment::RenderImmediate } {}
        // {} => null
        void ExecuteOnRenderAccessor(uint64_t uid, CefRefPtr<CefValue> pArgObj, cef::DataBindAccessor& accessor) const override
        {
            accessor.LaunchKernel();
            accessor.ResolveAsyncEndpoint(uid, true, CefValueNull());
        }
    };
}