// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "LinePlotElement.h"
#include "TextElement.h"
#include "GraphData.h"
#include <cmath>
#include <algorithm>
#include <functional>
#include <Core/source/infra/log/Logging.h>
#include <Core/source/gfx/layout/style/StyleProcessor.h>
#include <Core/source/pmon/Timekeeper.h>
#include <Core/source/infra/util/Assert.h>


namespace p2c::gfx::lay
{
	LinePlotElement::LinePlotElement(std::vector<std::shared_ptr<GraphLinePack>> packs_, std::vector<std::string> classes_)
		:
		PlotElement{ {}, std::move(classes_) },
		packs{ std::move(packs_) }
	{}

	LinePlotElement::~LinePlotElement() {}

	void LinePlotElement::Draw_(Graphics& gfx) const
	{
		const auto port = GetContentRect();
		const auto dims = port.GetDimensions();
		const auto yScaleLeft = (dims.height - 1) / (maxValueLeft - minValueLeft);
		const auto yBiasLeft = minValueLeft;
		const auto yScaleRight = (dims.height - 1) / (maxValueRight - minValueRight);
		const auto yBiasRight = minValueRight;
		const auto yOffset = port.bottom - 1;
		const auto xScale = dims.width / timeWindow;
		const auto xBias = pmon::Timekeeper::GetLockedNow();
		const auto xOffset = port.right;

		const auto ComputeScreen = [&](const DataPoint& d, AxisAffinity ax) {
			if (ax == AxisAffinity::Left) {
				return Vec2{
					xOffset - xScale * float(xBias - d.time),
					yOffset - yScaleLeft * (d.value.value_or(0.f) - yBiasLeft)
				};
			}
			else {
				return Vec2{
					xOffset - xScale * float(xBias - d.time),
					yOffset - yScaleRight * (d.value.value_or(0.f) - yBiasRight)
				};
			}
		};

		DrawGrid(gfx, port, hDivs, vDivs, gridColor);

		for (const auto& pack : packs)
		{
			const auto& pData = pack->data;
			const auto dataSize = pData->Size();
			const auto& data = *pData;
			if (dataSize >= 2)
			{
				const auto cutoff = xBias - timeWindow;
				// fill 
				if (pack->fillColor.a != 0.f)
				{
					const auto MakePeak = [&](const DataPoint& data)
					{
						const auto top = ComputeScreen(data, pack->axisAffinity);
						const auto bottom = Vec2{ top.x, port.bottom };
						return std::pair{ top, bottom };
					};

					gfx.FastTriangleBatchStart(port);
					{
						const auto p = MakePeak(data.Front());
						gfx.FastPeakStart(p.first, p.second, pack->fillColor);
					}
					size_t i = 1;
					for (; i < dataSize - 1; i++)
					{
						const auto p = MakePeak(data[i]);
						gfx.FastPeakAdd(p.first, p.second);
					}
					{
						const auto p = MakePeak(data[i]);
						gfx.FastPeakEnd(p.first, p.second);
					}
					gfx.FastBatchEnd();
				}
				// line 
				if (pack->lineColor.a != 0.f)
				{
					gfx.FastLineBatchStart(port, aa);
					gfx.FastLineStart(ComputeScreen(data.Front(), pack->axisAffinity), pack->lineColor);
					size_t i = 1;
					for (; i < dataSize - 1; i++)
					{
						gfx.FastLineAdd(ComputeScreen(data[i], pack->axisAffinity));
					}
					gfx.FastLineEnd(ComputeScreen(data[i], pack->axisAffinity));
					gfx.FastBatchEnd();
				}
			}
		}
	}

	void LinePlotElement::SetPosition_(const Vec2& pos, const Dimensions& dimensions, sty::StyleProcessor& sp, Graphics& gfx)
	{
		gridColor = sp.Resolve<sty::at::graphGridColor>();
		hDivs = sp.Resolve<sty::at::graphHorizontalDivs>();
		vDivs = sp.Resolve<sty::at::graphVerticalDivs>();
		aa = sp.Resolve<sty::at::graphAntiAlias>();
		PlotElement::SetPosition_(pos, dimensions, sp, gfx);
	}

	void LinePlotElement::SetValueRangeLeft(float min, float max)
	{
		minValueLeft = min;
		maxValueLeft = max;
	}

	void LinePlotElement::SetValueRangeRight(float min, float max)
	{
		minValueRight = min;
		maxValueRight = max;
	}

	void LinePlotElement::SetTimeWindow(float dt)
	{
		timeWindow = dt;
	}
}