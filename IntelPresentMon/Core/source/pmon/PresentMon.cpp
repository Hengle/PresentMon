// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "PresentMon.h"
#include <Core/source/infra/log/Logging.h>
#include <PresentMonAPI/PresentMonAPI.h>
#include "metric/NoisySineFakeMetric.h"
#include "metric/RawMetric.h"
#include "metric/SimpleMetric.h"
#include "metric/StatMetric.h"
#include "metric/StatArrayMetric.h"
#include "metric/PresentModeMetric.h"
#include "RawFrameDataWriter.h"
#include "metric/PsuArrayMetric.h"
#include "metric/PsuTypeArrayMetric.h"
#include "metric/RawArrayMetric.h"
#include "metric/InfoMetric.h"
#include "metric/SquareWaveMetric.h"

namespace p2c::pmon
{
	PresentMon::PresentMon(double window_in, double offset_in, uint32_t telemetrySamplePeriodMs_in)
		:
		fpsAdaptor{ this },
		gfxLatencyAdaptor{ this },
		gpuAdaptor{ this },
		rawAdaptor{ this },
		cpuAdaptor{ this },
		infoAdaptor{ this }
	{
		if (auto sta = pmInitialize(); sta != PM_STATUS::PM_STATUS_SUCCESS)
		{
			p2clog.note(L"could not init pmon").code(sta).commit();
		}

		// Build table of available metrics
		using namespace met;
		using namespace adapt;

		AddMetrics(StatMetric<&FpsAdapter::Struct::presented_fps>::MakeAll(1.f, L"Presented FPS", L"FPS", &fpsAdaptor));
		AddMetrics(StatMetric<&FpsAdapter::Struct::displayed_fps>::MakeAll(1.f, L"Displayed FPS", L"FPS", &fpsAdaptor));
		AddMetrics(StatMetric<&FpsAdapter::Struct::frame_time_ms>::MakeAll(1.f, L"Frame Time", L"ms", &fpsAdaptor));
		AddMetrics(StatMetric<&FpsAdapter::Struct::gpu_busy>::MakeAll(1.f, L"GPU Busy", L"ms", &fpsAdaptor));
		AddMetrics(StatMetric<&FpsAdapter::Struct::percent_dropped_frames>::MakeAll(1.f, L"Dropped Frames", L"%", &fpsAdaptor));
		AddMetric(std::make_unique<PresentModeMetric>(&fpsAdaptor));
		AddMetric(std::make_unique<SimpleMetric<&FpsAdapter::Struct::sync_interval>>(1.f, L"Sync Interval", L"vblk", &fpsAdaptor));
		AddMetric(std::make_unique<SimpleMetric<&FpsAdapter::Struct::allows_tearing>>(1.f, L"Allows Tearing", L"", &fpsAdaptor));

		AddMetrics(StatMetric<&GfxLatencyAdapter::Struct::display_latency_ms>::MakeAll(1.f, L"Gfx Latency Display", L"ms", &gfxLatencyAdaptor));
		AddMetrics(StatMetric<&GfxLatencyAdapter::Struct::render_latency_ms>::MakeAll(1.f, L"Gfx Latency Render", L"ms", &gfxLatencyAdaptor));

		AddMetrics(StatMetric<&GpuAdapter::Struct::gpu_power_w>::MakeAll(1.f, L"GPU Power", L"W", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::gpu_sustained_power_limit_w>::MakeAll(1.f, L"GPU Sust. Power Limit", L"W", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::gpu_voltage_v>::MakeAll(1.f, L"GPU Voltage", L"V", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::gpu_frequency_mhz>::MakeAll(1.f, L"GPU Frequency", L"MHz", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::gpu_temperature_c>::MakeAll(1.f, L"GPU Temperature", L"C", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::gpu_utilization>::MakeAll(1.f, L"GPU Utilization", L"%", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::gpu_power_limited>::MakeAll(100.f, L"GPU Power Limited", L"%", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::gpu_temperature_limited>::MakeAll(100.f, L"GPU Temp. Limited", L"%", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::gpu_current_limited>::MakeAll(100.f, L"GPU Current Limited", L"%", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::gpu_voltage_limited>::MakeAll(100.f, L"GPU Voltage Limited", L"%", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::gpu_utilization_limited>::MakeAll(100.f, L"GPU Util. Limited", L"%", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::vram_power_limited>::MakeAll(100.f, L"VRAM Power Limited", L"%", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::vram_temperature_limited>::MakeAll(100.f, L"VRAM Temp. Limited", L"%", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::vram_current_limited>::MakeAll(100.f, L"VRAM Current Limited", L"%", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::vram_voltage_limited>::MakeAll(100.f, L"VRAM Voltage Limited", L"%", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::vram_utilization_limited>::MakeAll(100.f, L"VRAM Util. Limited", L"%", &gpuAdaptor));

		AddMetrics(StatMetric<&CpuAdapter::Struct::cpu_utilization>::MakeAll(1.f, L"CPU Utilization", L"%", &cpuAdaptor));
		AddMetrics(StatMetric<&CpuAdapter::Struct::cpu_power_w>::MakeAll(1.f, L"CPU Power", L"W", &cpuAdaptor));
		AddMetrics(StatMetric<&CpuAdapter::Struct::cpu_power_limit_w>::MakeAll(1.f, L"CPU Power Limit", L"W", &cpuAdaptor));
		AddMetrics(StatMetric<&CpuAdapter::Struct::cpu_temperature_c>::MakeAll(1.f, L"CPU Temperature", L"C", &cpuAdaptor));
		AddMetrics(StatMetric<&CpuAdapter::Struct::cpu_frequency>::MakeAll(.001f, L"CPU Frequency", L"GHz", &cpuAdaptor));

		AddMetric(std::make_unique<InfoMetric<&InfoAdapter::GetDateTime>>(L"Date Time", &infoAdaptor));
		AddMetric(std::make_unique<InfoMetric<&InfoAdapter::GetElapsedTime>>(L"Elapsed Time", &infoAdaptor));
		AddMetric(std::make_unique<InfoMetric<&InfoAdapter::GetGpuName>>(L"GPU Name", &infoAdaptor));

		AddMetrics(StatArrayMetric<&GpuAdapter::Struct::gpu_fan_speed_rpm>::MakeAllArray(1.f, L"GPU Fan Speed", L"RPM", MAX_PM_FAN_COUNT, &gpuAdaptor));

		AddMetrics(StatMetric<&GpuAdapter::Struct::gpu_render_compute_utilization>::MakeAll(1.f, L"GPU Render/Comp. Util.", L"%", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::gpu_media_utilization>::MakeAll(1.f, L"GPU Media Util.", L"%", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::vram_power_w>::MakeAll(1.f, L"VRAM Power", L"W", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::vram_voltage_v>::MakeAll(1.f, L"VRAM Voltage", L"V", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::vram_frequency_mhz>::MakeAll(0.0009765625f, L"VRAM Frequency", L"GHz", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::vram_effective_frequency_gbps>::MakeAll(0.0009765625f, L"VRAM Freq. Effective BW", L"GBps", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::vram_read_bandwidth_bps>::MakeAll(9.3132257e-10f, L"VRAM Read Bandwidth", L"GBps", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::vram_write_bandwidth_bps>::MakeAll(9.3132257e-10f, L"VRAM Write Bandwidth", L"GBps", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::vram_temperature_c>::MakeAll(1.f, L"VRAM Temperature", L"C", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::gpu_mem_total_size_b>::MakeAll(9.3132257e-10f, L"GPU Memory Size", L"GB", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::gpu_mem_used_b>::MakeAll(9.3132257e-10f, L"GPU Memory Used", L"GB", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::gpu_mem_read_bandwidth_bps>::MakeAll(9.3132257e-10f, L"GPU Memory Read BW", L"GBps", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::gpu_mem_write_bandwidth_bps>::MakeAll(9.3132257e-10f, L"GPU Memory Write BW", L"GBps", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::gpu_mem_max_bandwidth_bps>::MakeAll(9.3132257e-10f, L"GPU Memory Max BW", L"GBps", &gpuAdaptor));
		AddMetrics(StatMetric<&GpuAdapter::Struct::gpu_mem_utilization>::MakeAll(1.f, L"GPU Memory Used", L"%", &gpuAdaptor));

		AddMetrics(PsuTypeArrayMetric::MakeArray(MAX_PM_PSU_COUNT, &gpuAdaptor));
		AddMetrics(PsuArrayMetric<&PM_PSU_DATA::psu_power>::MakeAllArray(1.f, L"GPU PSU Power", L"W", MAX_PM_PSU_COUNT, &gpuAdaptor));
		AddMetrics(PsuArrayMetric<&PM_PSU_DATA::psu_voltage>::MakeAllArray(1.f, L"GPU PSU Voltage", L"V", MAX_PM_PSU_COUNT, &gpuAdaptor));

		AddMetric(std::make_unique<InfoMetric<&InfoAdapter::GetCpuName>>(L"CPU Name", &infoAdaptor));

		// fake metrics for testing
#ifdef _DEBUG
		AddMetric(std::make_unique<NoisySineFakeMetric>(L"Fake Noisy Sinusoid", 0.1f, 0.f, 30.f, 50.f, 3.f, 3.f));
		AddMetric(std::make_unique<NoisySineFakeMetric>(L"Fake Pure Sinusoid", 2.f, 0.f, 40.f, 50.f, 3.f, 0.f));
		AddMetric(std::make_unique<NoisySineFakeMetric>(L"Fake Slow Sinusoid", 0.05f, 0.f, 40.f, 50.f, 3.f, 0.f));
		AddMetric(std::make_unique<SquareWaveMetric>(L"Fake Square Wave", 1., 0.f, 100.f));
#endif

		// establish initial sampling / window / processing setting values
		SetWindow(window_in);
		SetOffset(offset_in);
		SetGpuTelemetryPeriod(telemetrySamplePeriodMs_in);
	}
	PresentMon::~PresentMon()
	{
		if (pid) {
			try { StopStream(); }
			catch (...) {}
		}
		if (auto sta = pmShutdown(); sta != PM_STATUS::PM_STATUS_SUCCESS) {
			p2clog.warn(L"could not shutdown pmon").code(sta).commit();
		}
	}
	void PresentMon::StartStream(uint32_t pid_)
	{
		if (pid) {
			if (*pid == pid_) {
				return;
			}
			p2clog.warn(std::format(L"Starting stream [{}] while previous stream [{}] still active", pid_, *pid)).commit();
			StopStream();
		}
		if (auto sta = pmStartStream(pid_); sta != PM_STATUS::PM_STATUS_SUCCESS) {
			p2clog.note(std::format(L"could not start stream for pid {}", pid_)).code(sta).commit();
		}
		pid = pid_;
		infoAdaptor.CaptureState();
		p2clog.info(std::format(L"started pmon stream for pid {}", *pid)).commit();
	}
	void PresentMon::StopStream()
	{
		if (!pid) {
			p2clog.warn(L"Cannot stop stream: no stream active").commit();
			return;
		}
		if (auto sta = pmStopStream(*pid); sta != PM_STATUS::PM_STATUS_SUCCESS) {
			p2clog.warn(std::format(L"could not stop stream for {}", *pid)).code(sta).commit();
		}
		cpuAdaptor.ClearCache();
		fpsAdaptor.ClearCache();
		gfxLatencyAdaptor.ClearCache();
		gpuAdaptor.ClearCache();
		rawAdaptor.ClearCache();
		p2clog.info(std::format(L"stopped pmon stream for pid {}", *pid)).commit();
		pid.reset();
	}
	double PresentMon::GetWindow() const { return window; }
	void PresentMon::SetWindow(double window_) { window = window_; }
	double PresentMon::GetOffset() const { return offset; }
	void PresentMon::SetOffset(double offset_)
	{
		if (auto sta = pmSetMetricsOffset(offset_); sta != PM_STATUS::PM_STATUS_SUCCESS)
		{
			p2clog.warn(std::format(L"could not set metrics offset to {}", offset)).code(sta).commit();
		}
		else
		{
			offset = offset_;
		}
	}
	void PresentMon::SetGpuTelemetryPeriod(uint32_t period)
	{
		if (auto sta = pmSetGPUTelemetryPeriod(period); sta != PM_STATUS::PM_STATUS_SUCCESS)
		{
			p2clog.warn(std::format(L"could not set gpu telemetry sample period to {}", offset)).code(sta).commit();
		}
		else
		{
			telemetrySamplePeriod = period;
		}
	}
	uint32_t PresentMon::GetGpuTelemetryPeriod()
	{
		return telemetrySamplePeriod;
	}
	std::wstring PresentMon::GetCpuName() const
	{
		char buffer[512];
		uint32_t bufferSize = sizeof(buffer);
		if (auto sta = pmGetCpuName(buffer, &bufferSize); sta != PM_STATUS_SUCCESS) {
			p2clog.warn(L"could not get cpu name").code(sta).commit();
			return {};
		}
		if (bufferSize >= sizeof(buffer)) {
			p2clog.warn(std::format(L"insufficient buffer size to get cpu name. written: {}", bufferSize)).commit();
		}
		return infra::util::ToWide(std::string{ buffer, bufferSize });
	}
	Metric* PresentMon::GetMetricByIndex(size_t index) { return metrics.at(index).get(); };
	std::vector<Metric::Info> PresentMon::EnumerateMetrics() const
	{
		std::vector<Metric::Info> info;
		size_t index = 0;
		for (auto& m : metrics)
		{
			info.push_back(m->GetInfo(index));
			index++;
		}
		return info;
	}
	std::vector<PresentMon::AdapterInfo> PresentMon::EnumerateAdapters() const
	{
		uint32_t count = 0;
		if (auto sta = pmEnumerateAdapters(nullptr, &count); sta != PM_STATUS::PM_STATUS_SUCCESS)
		{
			p2clog.note(L"could not enumerate adapter count").code(sta).commit();
		}
		std::vector<PM_ADAPTER_INFO> buffer{ size_t(count) };
		if (auto sta = pmEnumerateAdapters(buffer.data(), &count); sta != PM_STATUS::PM_STATUS_SUCCESS)
		{
			p2clog.note(L"could not enumerate adapter count").code(sta).commit();
		}
		std::vector<AdapterInfo> infos;
		for (const auto& info : buffer)
		{
			const auto GetVendorName = [vendor = info.vendor] {
				using namespace std::string_literals;
				switch (vendor) {
				case PM_GPU_VENDOR::PM_GPU_VENDOR_AMD: return "AMD"s;
				case PM_GPU_VENDOR::PM_GPU_VENDOR_INTEL: return "Intel"s;
				case PM_GPU_VENDOR::PM_GPU_VENDOR_NVIDIA: return "Nvidia"s;
				default: return "Unknown"s;
				}
			};
			infos.push_back(AdapterInfo{
				.id = info.id,
				.vendor = GetVendorName(),
				.name = info.name,
			});
		}
		return infos;
	}
	void PresentMon::SetAdapter(uint32_t id)
	{
		if (auto sta = pmSetActiveAdapter(id); sta != PM_STATUS::PM_STATUS_SUCCESS) {
			p2clog.note(L"could not set active adapter").code(sta).nox().commit();
		}
		else {
			selectedAdapter = id;
		}
	}
	std::optional<uint32_t> PresentMon::GetPid() const { return pid; }
	std::shared_ptr<RawFrameDataWriter> PresentMon::MakeRawFrameDataWriter(std::wstring path, std::optional<std::wstring> statsPath)
	{
		return std::make_shared<RawFrameDataWriter>(std::move(path), &rawAdaptor, std::move(statsPath));
	}
	void PresentMon::FlushRawData()
	{
		for (double t = -1.;; t -= 1.) {
			if (rawAdaptor.Pull(t).empty()) break;
		}
	}
	std::optional<uint32_t> PresentMon::GetSelectedAdapter() const
	{
		return selectedAdapter;
	}
	void PresentMon::AddMetric(std::unique_ptr<Metric> metric_)
	{
		metrics.push_back(std::move(metric_));
	}
	void PresentMon::AddMetrics(std::vector<std::unique_ptr<Metric>> metrics_)
	{
		metrics.insert(
			metrics.end(),
			std::make_move_iterator(metrics_.begin()),
			std::make_move_iterator(metrics_.end())
		);
	}
}