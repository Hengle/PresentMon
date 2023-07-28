// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <concepts>
#include "StatisticsTracker.h"
#include <PresentMonAPI/PresentMonAPI.h>
#include <Core/source/infra/log/Logging.h>
#include <Core/source/infra/util/Util.h>

namespace p2c::pmon
{
    namespace adapt
    {
        class RawAdapter;
    }

	template<typename T>
	concept PresentMonOptionalStrict_ =
		std::same_as<PM_FRAME_DATA_OPT_UINT64, T> ||
		std::same_as<PM_FRAME_DATA_OPT_DOUBLE, T> ||
		std::same_as<PM_FRAME_DATA_OPT_INT, T> ||
		std::same_as<PM_FRAME_DATA_OPT_PSU_TYPE, T>;

	template<typename T>
	concept PresentMonOptional = PresentMonOptionalStrict_<std::remove_cvref_t<T>>;

	class RawFrameDataWriter
	{
	public:
        RawFrameDataWriter(std::wstring path, adapt::RawAdapter* pAdapter, std::optional<std::wstring> frameStatsPath);
		RawFrameDataWriter(const RawFrameDataWriter&) = delete;
		RawFrameDataWriter& operator=(const RawFrameDataWriter&) = delete;
		void Process(double timestamp);
		~RawFrameDataWriter();
	private:
		// types
		class FileStream_
		{
		public:
			FileStream_(const std::wstring& path) { file_.open(path, std::ios::trunc); }
			template<PresentMonOptional T>
			FileStream_& operator<<(const T& input)
			{
				if (!input.valid) {
					file_ << "NA";
				}
				else {
					file_ << input.data;
				}
				return *this;
			}
			template<typename T>
			FileStream_& operator<<(T&& input)
			{
				file_ << std::forward<T>(input);
				return *this;
			}
		private:
			std::ofstream file_;
		};
		// functions
		double GetDuration_() const;
		void WriteStats_();
		// data
		adapt::RawAdapter* pAdapter;
		std::optional<std::wstring> frameStatsPath;
		std::unique_ptr<StatisticsTracker> pStatsTracker;
		double startTime = -1.;
		double endTime = -1.;
		FileStream_ file;
	};
}
