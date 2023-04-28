// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "SimpleFileDriver.h"

namespace p2c::infra::log::drv
{
	SimpleFileDriver::SimpleFileDriver(std::filesystem::path path)
	{
		std::filesystem::create_directories(path.parent_path());
		file.open(path, file.out | file.app);
	}

	void SimpleFileDriver::Commit(const EntryOutputBase& entry)
	{
		file << FormatEntry(entry);
		if (entry.flushing)
		{
			file.flush();
		}
	}
}