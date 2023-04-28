// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include "../AsyncEndpoint.h"
#include <Core/source/kernel/Kernel.h>
#include "../CefValues.h"

namespace p2c::client::util::async
{
    class SetAdapter : public AsyncEndpoint
    {
    public:
        static constexpr std::string GetKey() { return "setAdapter"; }
        SetAdapter() : AsyncEndpoint{ AsyncEndpoint::Environment::KernelTask } {}
        // {id:int} => null
        Result ExecuteOnKernelTask(uint64_t uid, CefRefPtr<CefValue> pArgObj, kern::Kernel& kernel) const override
        {
            kernel.SetAdapter(Traverse(pArgObj)["id"]);
            return Result{ true, CefValueNull() };
        }
    };
}