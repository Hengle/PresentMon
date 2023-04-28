// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "GraphData.h"
#include <algorithm>

namespace p2c::gfx::lay
{
	GraphData::GraphData(double timeWindow)
		:
		timeWindow{ timeWindow }
	{}
	DataPoint& GraphData::operator[](size_t i)
	{
		return data[i];
	}
	const DataPoint& GraphData::operator[](size_t i) const
	{
		return data[i];
	}
	DataPoint& GraphData::Front()
	{
		return data.front();
	}
	const DataPoint& GraphData::Front() const
	{
		return data.front();
	}
	DataPoint& GraphData::Back()
	{
		return data.back();
	}
	const DataPoint& GraphData::Back() const
	{
		return data.back();
	}
	void GraphData::Push(const DataPoint& dp)
	{
		data.push_front(dp);
		min.Push(dp.value);
		max.Push(dp.value);
	}
	size_t GraphData::Size() const
	{
		return data.size();
	}
	void GraphData::Trim(double now)
	{
		const auto cutoff = now - timeWindow;
		while (!data.empty() && data.back().time < cutoff) {
			const auto v = data.back().value;
			min.Pop(v);
			max.Pop(v);
			data.pop_back();
		}
	}
	void GraphData::Resize(double window)
	{
		timeWindow = window;
	}
	std::optional<float> GraphData::Min() const
	{
		return min.GetCurrent();
	}
	std::optional<float> GraphData::Max() const
	{
		return max.GetCurrent();
	}
}