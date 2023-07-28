// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include "../AsyncEndpoint.h"
#include <Core/source/kernel/Kernel.h>
#include "../CefValues.h"

namespace p2c::client::util::async
{
    class SetCapture : public AsyncEndpoint
    {
    public:
        static constexpr std::string GetKey() { return "setCapture"; }
        SetCapture() : AsyncEndpoint{ AsyncEndpoint::Environment::KernelTask } {}
        // {active:bool} => null
        Result ExecuteOnKernelTask(uint64_t uid, CefRefPtr<CefValue> pArgObj, kern::Kernel& kernel) const override
        {
            kernel.SetCapture(Traverse(pArgObj)["active"]);
            return Result{ true, CefValueNull() };
        }
    };
}