// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <optional>
#include <vector>
#include <memory>
#include "metric/Metric.h"
// adapters corresponding to pmapi structs/functions
#include "adapt/FpsAdapter.h"
#include "adapt/GfxLatencyAdapter.h"
#include "adapt/GpuAdapter.h"
#include "adapt/RawAdapter.h"
#include "adapt/CpuAdapter.h"
#include "adapt/InfoAdapter.h"
#include "AdapterInfo.h"

namespace p2c::pmon
{
	class RawFrameDataWriter;

	class PresentMon
	{
	public:
		// types
		using AdapterInfo = pmon::AdapterInfo;
		// functions
		PresentMon(double window = 1000., double offset = 1000., uint32_t telemetrySampleRateMs = 16);
		~PresentMon();
		void StartStream(uint32_t pid_);
		void StopStream();
		double GetWindow() const;
		void SetWindow(double window_);
		double GetOffset() const;
		void SetOffset(double offset_);
		void SetGpuTelemetryPeriod(uint32_t period);
		uint32_t GetGpuTelemetryPeriod();
		std::wstring GetCpuName() const;
		std::vector<AdapterInfo> EnumerateAdapters() const;
		void SetAdapter(uint32_t id);
		Metric* GetMetricByIndex(size_t index);
		std::vector<Metric::Info> EnumerateMetrics() const;
		std::optional<uint32_t> GetPid() const;
		std::shared_ptr<RawFrameDataWriter> MakeRawFrameDataWriter(std::wstring path, std::optional<std::wstring> statsPath);
		void FlushRawData();
		std::optional<uint32_t> GetSelectedAdapter() const;
	private:
		// functions
		void AddMetric(std::unique_ptr<Metric> metric_);
		void AddMetrics(std::vector<std::unique_ptr<Metric>> metrics_);
		// data
		double window = -1.;
		double offset = -1.;
		uint32_t telemetrySamplePeriod = 0;
		adapt::FpsAdapter fpsAdaptor;
		adapt::GfxLatencyAdapter gfxLatencyAdaptor;
		adapt::GpuAdapter gpuAdaptor;
		adapt::RawAdapter rawAdaptor;
		adapt::CpuAdapter cpuAdaptor;
		adapt::InfoAdapter infoAdaptor;
		std::vector<std::unique_ptr<Metric>> metrics;
		std::optional<uint32_t> pid;
		std::optional<uint32_t> selectedAdapter;
	};
}