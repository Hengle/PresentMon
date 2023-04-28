// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "NanoCefBrowserClient.h"
#include <Core/source/kernel/Kernel.h>
#include <include/wrapper/cef_helpers.h>
#include "NanoCefProcessHandler.h"
#include "SchemeHandlerFactory.h"
#include "DataBindAccessor.h"
#include <Core/source/infra/svc/Services.h>
#include "util/CefProcessCompass.h"
#include <Core/source/infra/log/DefaultChannel.h>
#include <Core/source/infra/log/Channel.h>
#include <Core/source/infra/log/drv/DebugOutDriver.h>
#include "util/CefIpcLogDriver.h"
#include "include/wrapper/cef_closure_task.h"
#include <include/cef_task.h>
#include "include/base/cef_callback.h"
#include "util/AsyncEndpointManager.h"
#include "util/CefValues.h"


namespace p2c::client::cef
{
    using namespace infra::svc;

    void NanoCefProcessHandler::OnContextInitialized()
    {
        CefRegisterSchemeHandlerFactory("https", "app", new SchemeHandlerFactory{});
    }

    CefRefPtr<CefBrowserProcessHandler> NanoCefProcessHandler::GetBrowserProcessHandler()
    {
        return this;
    }

    CefRefPtr<CefRenderProcessHandler> NanoCefProcessHandler::GetRenderProcessHandler()
    {
        return this;
    }

    void NanoCefProcessHandler::OnBeforeChildProcessLaunch(CefRefPtr<CefCommandLine> pChildCommandLine)
    {
        // propagate cli switches to children
        auto pCmdLine = CefCommandLine::GetGlobalCommandLine();
        if (pCmdLine->HasSwitch("p2c-verbose"))
        {
            pChildCommandLine->AppendSwitch("p2c-verbose");
        }
        if (pCmdLine->HasSwitch("p2c-files-working"))
        {
            pChildCommandLine->AppendSwitch("p2c-files-working");
        }
        if (pCmdLine->HasSwitch("p2c-log-black"))
        {
            pChildCommandLine->AppendSwitch("p2c-log-black");
        }
    }

    void NanoCefProcessHandler::AddFunctionToObject_(CefString name, CefRefPtr<CefV8Value>& pObj, CefRefPtr<DataBindAccessor>& pAccessor)
    {
        pObj->SetValue(name, CefV8Value::CreateFunction(name, pAccessor), V8_PROPERTY_ATTRIBUTE_NONE);
    }

    void NanoCefProcessHandler::OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context)
    {
        pAccessor = new DataBindAccessor{ pBrowser, pKernelWrapper.get() };

        auto core = CefV8Value::CreateObject(nullptr, nullptr);
        AddFunctionToObject_("invokeEndpoint", core, pAccessor);
        AddFunctionToObject_("registerSignalHandler", core, pAccessor);
        context->GetGlobal()->SetValue("core", std::move(core), V8_PROPERTY_ATTRIBUTE_NONE);
    }

    void NanoCefProcessHandler::OnBrowserCreated(CefRefPtr<CefBrowser> browser_, CefRefPtr<CefDictionaryValue> extra_info)
    {
        pBrowser = browser_;

        if (!Services::Resolve<client::util::CefProcessCompass>()->IsClient())
        {
            // setup ipc logging
            auto pChan = infra::log::GetDefaultChannel();
            pChan->ClearDrivers();
            pChan->AddDriver(std::make_unique<util::log::CefIpcLogDriver>(pBrowser));
            pChan->AddDriver(std::make_unique<infra::log::drv::DebugOutDriver>());
        }

        CefRenderProcessHandler::OnBrowserCreated(std::move(browser_), std::move(extra_info));
    }

    bool NanoCefProcessHandler::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId source_process, CefRefPtr<CefProcessMessage> message)
    {
        if (message->GetName() == util::AsyncEndpointManager::GetResolveMessageName())
        {
            pAccessor->ResolveAsyncEndpoint(
                message->GetArgumentList()->GetInt(0),
                message->GetArgumentList()->GetBool(1),
                message->GetArgumentList()->GetValue(2)
            );
            return true;
        }
        else if (message->GetName() == GetShutdownMessageName())
        {
            // release important resources
            pAccessor->ClearKernelWrapper();
            pKernelWrapper.reset();

            // send shutdown ack
            auto pMsg = CefProcessMessage::Create(GetShutdownMessageName());
            pBrowser->GetMainFrame()->SendProcessMessage(PID_BROWSER, std::move(pMsg));

            return true;
        }
        return false;
    }
}