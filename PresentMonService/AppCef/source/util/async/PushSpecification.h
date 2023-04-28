// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include "../AsyncEndpoint.h"
#include <Core/source/kernel/Kernel.h>
#include "../CefValues.h"
#include "../MakeOverlaySpec.h"

namespace p2c::client::util::async
{
    class PushSpecification : public AsyncEndpoint
    {
    public:
        static constexpr std::string GetKey() { return "pushSpecification"; }
        PushSpecification() : AsyncEndpoint{ AsyncEndpoint::Environment::KernelTask } {}
        // {very-complicated-spec-object} => null
        Result ExecuteOnKernelTask(uint64_t uid, CefRefPtr<CefValue> pArgObj, kern::Kernel& kernel) const override
        {
            if (Traverse(pArgObj)["pid"].IsNull())
            {
                kernel.ClearOverlay();
            }
            else
            {
                kernel.PushSpec(MakeOverlaySpec(std::move(pArgObj)));
            }
            return Result{ true, CefValueNull() };
        }
    };
}