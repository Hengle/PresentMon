// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include "../AsyncEndpoint.h"
#include <Core/source/kernel/Kernel.h>
#include "../CefValues.h"

namespace p2c::client::util::async
{
    class EnumerateProcesses : public AsyncEndpoint
    {
    public:
        static constexpr std::string GetKey() { return "enumerateProcesses"; }
        EnumerateProcesses() : AsyncEndpoint{ AsyncEndpoint::Environment::KernelTask } {}
        // {} => {processes: [{name: string, pid: uint}]}
        Result ExecuteOnKernelTask(uint64_t uid, CefRefPtr<CefValue> pArgObj, kern::Kernel& kernel) const override
        {
            auto procList = kernel.ListProcesses();
            auto procListCef = MakeCefList(procList.size());
            for (int i = 0; i < procList.size(); i++)
            {
                auto& proc = procList[i];
                procListCef->SetValue(i, MakeCefObject(
                    CefProp{ "name", std::move(proc.name) },
                    CefProp{ "pid", proc.pid },
                    CefProp{ "windowName", std::move(proc.windowName) }
                ));
            }
            return Result{ true, MakeCefObject(CefProp{ "processes", CefValueDecay(std::move(procListCef)) }) };
        }
    };
}