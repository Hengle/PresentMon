// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include "../AsyncEndpoint.h"
#include <Core/source/kernel/Kernel.h>
#include "../CefValues.h"

namespace p2c::client::util::async
{
    class EnumerateAdapters : public AsyncEndpoint
    {
    public:
        static constexpr std::string GetKey() { return "enumerateAdapters"; }
        EnumerateAdapters() : AsyncEndpoint{ AsyncEndpoint::Environment::KernelTask } {}
        // {} => {adapters: [{id: uint, vendor: string, name: string}]}
        Result ExecuteOnKernelTask(uint64_t uid, CefRefPtr<CefValue> pArgObj, kern::Kernel& kernel) const override
        {
            auto adapters = kernel.EnumerateAdapters();

            auto list = CefListValue::Create();
            for (int i = 0; i < int(adapters.size()); i++)
            {
                list->SetValue(i, MakeCefObject(
                    CefProp{ "id", int(adapters[i].id) },
                    CefProp{ "vendor", std::move(adapters[i].vendor) },
                    CefProp{ "name", std::move(adapters[i].name) }
                ));
            }
            return Result{ true, MakeCefObject(CefProp{ "adapters", CefValueDecay(std::move(list)) })};
        }
    };
}