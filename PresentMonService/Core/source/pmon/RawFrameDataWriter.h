// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <fstream>

namespace p2c::pmon
{
    namespace adapt
    {
        class RawAdapter;
    }

	class RawFrameDataWriter
	{
	public:
        RawFrameDataWriter(std::wstring path, adapt::RawAdapter* pAdapter);
		void Process(double timestamp);
	private:
		adapt::RawAdapter* pAdapter;
		std::ofstream file;
	};
}
