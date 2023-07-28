// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "ComManager.h"
#include <Core/source/infra/log/Logging.h>
#include "Comdef.h"


namespace p2c::win::com
{
	ComManager::ComManager()
	{
        if (auto hr = CoInitializeEx(0, COINIT_MULTITHREADED); FAILED(hr))
        {
			p2clog.note(L"Failed to init COM").hr(hr).commit();
        }

		if (auto hr = CoInitializeSecurity(
            nullptr,
            -1,                          // COM negotiates service
            nullptr,                     // Authentication services
            nullptr,                     // Reserved
            RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication 
            RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation  
            nullptr,                     // Authentication info
            EOAC_NONE,                   // Additional capabilities 
            nullptr                      // Reserved
        ); FAILED(hr) && hr != RPC_E_TOO_LATE)
		{
            CoUninitialize();
			p2clog.note(L"Failed to init COM security").hr(hr).commit();
		}
	}

	ComManager::~ComManager()
	{
        CoUninitialize();
	}
}