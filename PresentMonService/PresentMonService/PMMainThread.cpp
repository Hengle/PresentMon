// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "Service.h"
#include "NamedPipeServer.h"
#include "PresentMon.h"
#include <PowerTelemetryProviderFactory.h>
#include "IntelCpu.h"
#include "WmiCpu.h"

bool NanoSleep(int32_t ms) {
  HANDLE timer;
  LARGE_INTEGER li;
  // Convert from ms to 100ns units and negate
  int64_t ns = -10000 * (int64_t)ms;
  // Create a high resolution table
  if (!(timer = CreateWaitableTimerEx(NULL, NULL,
                                      CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                      TIMER_ALL_ACCESS))) {
    return false;
  }
  li.QuadPart = ns;
  if (!SetWaitableTimer(timer, &li, 0, NULL, NULL, FALSE)) {
    CloseHandle(timer);
    return false;
  }
  WaitForSingleObject(timer, INFINITE);
  CloseHandle(timer);
  return true;
}

// Attempt to use a high resolution sleep but if not
// supported use regular Sleep().
void PmSleep(int32_t ms) {
  if (!NanoSleep(ms)) {
    Sleep(ms);
  }
  return;
}

void IPCCommunication(Service* srv, PresentMon* pm)
{
    bool createNamedPipeServer = true;

    if (srv == nullptr) {
        return;
    }

    NamedPipeServer* nps = new NamedPipeServer(srv, pm);
    if (nps == nullptr) {
        return;
    }

    while (createNamedPipeServer) {
            DWORD result = nps->RunServer();
            if (result == ERROR_SUCCESS) {
                createNamedPipeServer = false;
            }
            else {
                // We were unable to start our named pipe server. Sleep for
                // a bit and then try again.
                PmSleep(3000);
            }
    }

    delete nps;

    return;
}

void PowerTelemetry(Service* srv, PresentMon* pm, std::vector<std::shared_ptr<pwr::PowerTelemetryAdapter>>* pAdapters)
{
  if (pAdapters == nullptr || srv == nullptr || pm == nullptr) {
    // TODO: log error on this condition
    return;
  }

  // Get the streaming start event
  HANDLE events[2];
  events[0] = pm->GetStreamingStartHandle();
  events[1] = srv->GetServiceStopHandle();

  while (1) {
    auto waitResult = WaitForMultipleObjects(2, events, FALSE, INFINITE);
    auto i = waitResult - WAIT_OBJECT_0;
    if (i == 1) {
      return;
    }
    while (WaitForSingleObject(srv->GetServiceStopHandle(), 0) !=
           WAIT_OBJECT_0) {
      for (auto& adapter : *pAdapters) {
        adapter->Sample();
      }
      PmSleep(pm->GetGpuTelemetryPeriod());
      // Get the number of currently active streams
      auto num_active_streams = pm->GetActiveStreams();
      if (num_active_streams == 0) {
        break;
      }
    }
  }
}

void CpuTelemetry(Service* srv, PresentMon* pm,
                  std::shared_ptr<pwr::cpu::CpuTelemetry>* cpu) {
  if (srv == nullptr || pm == nullptr) {
    // TODO: log error on this condition
    return;
  }

  HANDLE events[2];
  events[0] = pm->GetStreamingStartHandle();
  events[1] = srv->GetServiceStopHandle();

  while (1) {
    auto waitResult = WaitForMultipleObjects(2, events, FALSE, INFINITE);
    auto i = waitResult - WAIT_OBJECT_0;
    if (i == 1) {
      return;
    }
    while (WaitForSingleObject(srv->GetServiceStopHandle(), 0) !=
            WAIT_OBJECT_0) {
      cpu->get()->Sample();
      PmSleep(pm->GetGpuTelemetryPeriod());
      // Get the number of currently active streams
      auto num_active_streams = pm->GetActiveStreams();
      if (num_active_streams == 0) {
        break;
      }
    }
  }
}

  DWORD WINAPI PresentMonMainThread(LPVOID lpParam)
{
    // TODO(megalvan): Need to set this as a debug flag to enable
    // attachment to a debugger. This should only be
    // available in DEBUG mode and then be keyed upon
    // a setting in the service control panel
    bool debug               = false;

    while (debug) {
        PmSleep(500);
    }

    if (lpParam == nullptr) {
        return ERROR_INVALID_DATA;
    }

    PresentMon pm;

    // Extract out the PresentMon Service pointer
    const auto srv = static_cast<Service*>(lpParam);

    // Grab the stop service event handle
    const auto serviceStopHandle = srv->GetServiceStopHandle();

    // Start IPC communication thread
    std::jthread ipc_thread(IPCCommunication, srv, &pm);

    // telemetry system
    std::vector<std::unique_ptr<pwr::PowerTelemetryProvider>> telemetry_providers;
    std::vector<std::shared_ptr<pwr::PowerTelemetryAdapter>> telemetry_adapters;
    std::jthread telemetry_thread;

    try {
        // create providers
        for (int iVendor = 0; iVendor < int(PM_GPU_VENDOR_UNKNOWN); iVendor++) {
            try {
                if (auto pProvider = pwr::PowerTelemetryProviderFactory::Make(PM_GPU_VENDOR(iVendor))) {
                    telemetry_providers.push_back(std::move(pProvider));
                }
            } catch (...) {} // silent fail (maybe log?) any provider construction exceptions and just keep the good ones
        }
        // collect all adapters together from providers
        for (const auto& pProvider : telemetry_providers) {
            auto& adapters = pProvider->GetAdapters();
            telemetry_adapters.insert(telemetry_adapters.end(), adapters.begin(), adapters.end());
        }
        // bail if there are not adapters
        if (telemetry_adapters.size() == 0) {
            throw std::runtime_error{ "no telemetry adapters" };
        }
        telemetry_thread = std::jthread{ PowerTelemetry, srv, &pm, &telemetry_adapters };
        pm.SetTelemetryAdapters(telemetry_adapters);
    } catch (...) {}
    
    // Create CPU telemetry
    std::shared_ptr<pwr::cpu::CpuTelemetry> cpu;
    std::jthread cpu_telemetry_thread;
    try {
      // First try to create an IntelCpu which uses IPF
      // for metrics sampling
      cpu = std::make_shared<pwr::cpu::intel::IntelCpu>();
    } catch (...) {
      try {
        // If not successful try to use WMI for metrics sampling
        cpu = std::make_shared<pwr::cpu::wmi::WmiCpu>();
      } catch (...) {
      }
    }

    if (cpu) {
      cpu_telemetry_thread = std::jthread{ CpuTelemetry, srv, &pm, &cpu };
      pm.SetCpu(cpu);
    }

    while (WaitForSingleObject(serviceStopHandle, 0) != WAIT_OBJECT_0) {
        pm.CheckTraceSessions();
        PmSleep(500);
    }

    // Stop the PresentMon session
    pm.StopTraceSession();

    return ERROR_SUCCESS;
}
