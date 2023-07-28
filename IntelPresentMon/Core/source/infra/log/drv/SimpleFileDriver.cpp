// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "SimpleFileDriver.h"

namespace p2c::infra::log::drv
{
	SimpleFileDriver::SimpleFileDriver(std::filesystem::path path)
	{
		const auto parentPath = path.parent_path();
		if (!parentPath.empty()) {
			std::filesystem::create_directories(parentPath);
		}
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