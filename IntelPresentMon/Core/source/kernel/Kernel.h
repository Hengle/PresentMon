// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <Core/source/win/Process.h>
#include <Core/source/pmon/PresentMon.h>
#include "OverlaySpec.h"
#include "KernelHandler.h"

#pragma comment(lib, "user32")
#pragma comment(lib, "winmm")

namespace p2c::gfx
{
    class Graphics;
    namespace lay
    {
        class GraphData;
    }
}

namespace p2c::kern
{
    struct OverlaySpec;
    class OverlayContainer;

    struct Process : public win::Process
    {
        Process(win::Process base) : win::Process{ std::move(base) } {}
        std::optional<std::wstring> windowName;
    };

    class Kernel
    {
    public:
        Kernel(KernelHandler* pHandler) noexcept;
        Kernel(const Kernel&) = delete;
        Kernel& operator=(const Kernel&) = delete;
        ~Kernel();
        void PushSpec(std::unique_ptr<OverlaySpec> pSpec);
        void ClearOverlay();
        std::vector<Process> ListProcesses();
        std::vector<pmon::Metric::Info> EnumerateMetrics() const;
        void SetAdapter(uint32_t id);
        std::vector<pmon::PresentMon::AdapterInfo> EnumerateAdapters() const;
        void SetCapture(bool active);
    private:
        // functions
        bool IsIdle_() const;
        std::unique_ptr<OverlaySpec> PullSpec_();
        // top level root acts like state machine for spawning/running overlay
        void ThreadProcedure_();
        // loop runs while overlay window active, holds message pump etc.
        void RunOverlayLoop_();
        void ConfigurePresentMon_(const OverlaySpec& newSpec);
        // data
        KernelHandler* pHandler = nullptr;
        std::optional<pmon::PresentMon> pm; // optional to defer creation to when the thread is run
        bool dying = false;
        bool clearRequested = false;
        bool inhibitTargetLostSignal = false;
        std::optional<bool> pushedCaptureActive;
        std::unique_ptr<OverlaySpec> pPushedSpec;
        std::unique_ptr<OverlayContainer> pOverlayContainer;
        mutable std::condition_variable cv;
        mutable std::mutex mtx;
        std::jthread thread;
    };
}