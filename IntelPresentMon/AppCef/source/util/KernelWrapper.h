#pragma once
#include <memory>
#include "HotkeyListener.h"
#include "SignalManager.h"
#include "AsyncEndpointManager.h"

namespace p2c::kern
{
    class Kernel;
    class KernelHandler;
}

namespace p2c::client::util
{
	struct KernelWrapper
	{
        KernelWrapper();
        ~KernelWrapper();
        util::SignalManager signals;
        util::AsyncEndpointManager asyncEndpoints;
        std::unique_ptr<kern::Kernel> pKernel;
        std::unique_ptr<kern::KernelHandler> pKernelHandler;
        std::unique_ptr<util::Hotkeys> pHotkeys;
	};
}
