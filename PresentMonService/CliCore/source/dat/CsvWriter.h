// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <fstream>
#include <memory>
#include <vector>
#include <sstream>
#include "FrameSink.h"

namespace p2c::cli::dat
{
	struct GroupFlags;

	class CsvWriter : public FrameSink
	{
	public:
        CsvWriter(std::string path, const std::vector<std::string>& groups, bool writeStdout = false);
		CsvWriter(const CsvWriter&) = delete;
		CsvWriter(CsvWriter&&);
		CsvWriter& operator=(const CsvWriter&) = delete;
		CsvWriter& operator=(CsvWriter&&);
		~CsvWriter();
		void Process(const struct PM_FRAME_DATA& frame) override;
	private:
		// functions
		void Flush();
		// data
		std::unique_ptr<GroupFlags> pGroupFlags_;
		std::stringstream buffer_;
		std::ofstream file_;
		bool writeStdout_;
	};
}
