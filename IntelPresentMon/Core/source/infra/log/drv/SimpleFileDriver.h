// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include "../Driver.h"
#include <filesystem>
#include <fstream>

namespace p2c::infra::log
{
	class EntryOutputBase;
}

namespace p2c::infra::log::drv
{
	class SimpleFileDriver : public Driver
	{
	public:
		SimpleFileDriver(std::filesystem::path path);
		void Commit(const EntryOutputBase&) override;
	private:
		std::wofstream file;
	};
}