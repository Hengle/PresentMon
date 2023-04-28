#pragma once

#include <shared_mutex>
#include <vector>
#include <windows.h>
#include "nvapi.h"

struct PerformanceData {
	uint64_t StartQpcTime;                // Start QPC value when the data was sampled
	unsigned long NvidiaGpuGraphicsClock; // Sampled Nvidia GPU Clock
	unsigned long NvidiaGpuTemperature;   // Sampled Nvidia GPU Temperature
	uint64_t EndQpcTime;                  // End QPC value when the data was sampled
};

class NvidiaDataSampler
{
public:
	NvidiaDataSampler();
	~NvidiaDataSampler();

	unsigned long GetGpuGraphicsClock(unsigned long gpuNumber);
	unsigned long GetGpuTemperature(unsigned long gpuNumber);

	unsigned long GetNumGpus()
	{
		return mGpuCount;
	}

	bool Initialize();
private:
	NvAPI_Status GetGPUs();

	NvPhysicalGpuHandle mGpuHandleArray[NVAPI_MAX_PHYSICAL_GPUS] = { 0 };
	NV_GPU_THERMAL_SETTINGS mThermalSettings;
	NV_GPU_CLOCK_FREQUENCIES mClockFrequencies;
	NvU32 mGpuCount;
	bool mInitialized;
};

class DataSampler
{
public:
	DataSampler();
	~DataSampler();

	bool EnableDataSinks();
	bool SampleData();

	void DequeuePerformanceData();
	void GetLatestPerformanceData(PerformanceData& outPerformanceData);
	void FindClosestPerformanceData(uint64_t QpcTime, PerformanceData& outPerformanceData);

private:

	std::mutex mPerformanceDataMutex;
	std::vector<std::shared_ptr<PerformanceData>> mPerformanceData;
	std::mutex mCachedDataMutex;
	std::vector<std::shared_ptr<PerformanceData>> mCachedData;
	int mNumSamples;

	PerformanceData mLatestPerformanceData;
	NvidiaDataSampler* mNvidiaSampler;
	bool mNvidiaAvailable;
	unsigned long mNvidiaNumGPUs;

	bool mIntelAvailable;
	bool mAmdAvailable;

};

void FindClosestPerformanceData(uint64_t QpcTime, PerformanceData& outPerformanceData);
void GetLatestSampledInfo(PerformanceData& outSampledInfo);
