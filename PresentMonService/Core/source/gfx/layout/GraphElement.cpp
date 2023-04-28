// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "GraphElement.h"
#include "TextElement.h"
#include "LinePlotElement.h"
#include "HistogramPlotElement.h"
#include "GraphData.h"
#include <cmath>
#include <format>
#include <Core/source/infra/log/Logging.h>
#include <Core/source/gfx/layout/style/StyleProcessor.h>
#include <ranges>
#include <Core/source/infra/util/rn/ToVector.h>


namespace p2c::gfx::lay
{
	using namespace std::string_literals;
	namespace rn = std::ranges;
	namespace vi = rn::views;

	// invisible element used so that we can target individual metrics (lines) with styles
	class GraphElement::VirtualLineElement : public Element
	{
	public:
		VirtualLineElement(GraphElement* pParent, size_t index)
			:
			Element{ { "$metric"s, std::format("$metric-{}", index) } },
			pParent{ pParent },
			index{ index }
		{}
	protected:
		// functions
		LayoutConstraints QueryLayoutConstraints_(std::optional<float> width, sty::StyleProcessor& sp, Graphics& gfx) const override
		{
			return LayoutConstraints{
				.min = 0.f,
				.max = 0.f,
				.basis = 0.f,
				.flexGrow = 0.f,
			};
		}
		void SetDimension_(float dimension, FlexDirection dir, sty::StyleProcessor& sp, Graphics& gfx) override {}
		void SetPosition_(const Vec2& pos, const Dimensions& dimensions, sty::StyleProcessor& sp, Graphics& gfx) override
		{
			pParent->packs[index]->lineColor = sp.Resolve<lay::sty::at::graphLineColor>();
			pParent->packs[index]->fillColor = sp.Resolve<lay::sty::at::graphFillColor>();
			pParent->packs[index]->axisAffinity = sp.Resolve<lay::sty::at::widgetMetricAxisAffinity>();
		}
		void Draw_(Graphics& gfx) const override {}
		// data
		GraphElement* pParent = nullptr;
		size_t index;
	};

	GraphElement::GraphElement(GraphType type_, std::vector<std::shared_ptr<GraphLinePack>> packs_, std::vector<std::string> classes_)
		:
		FlexElement{ {}, [&classes_, type_] {
			classes_.push_back("$graph");
			classes_.push_back(type_ == GraphType::Histogram ? "$hist" : "$line");
			return std::move(classes_);
		}() },
		packs{ std::move(packs_) }
	{
		// graph labels (title) and virtual line elements
		for (size_t i = 0; i < packs.size(); i++) {
			AddChild(FlexElement::Make({
				FlexElement::Make({}, {"$label-swatch", std::format("$metric-{}", i)}),
				TextElement::Make(packs[i]->label, {"$label"}),
			}, { "$label-wrap", std::format("$metric-label-{}", i)}));
			AddChild(std::make_shared<VirtualLineElement>(this, i));
			if (packs[i]->axisAffinity == AxisAffinity::Right) {
				isDualAxis = true;
			}
		}

		// body 
		const auto MakePlotElement = [&]() -> std::shared_ptr<PlotElement> {
			switch (type_)
			{
			case GraphType::Histogram:
				if (packs.size() > 1) p2clog.warn(L"Histogram with multiple data packs").commit();
				return std::make_shared<HistogramPlotElement>(packs.front(), std::vector<std::string>{"$body-plot"});
			case GraphType::Line: return std::make_shared<LinePlotElement>(packs, std::vector<std::string>{"$body-plot"});
			default: p2clog.note(L"Bad graph type").commit(); return {};
			}
		};
		AddChild( FlexElement::Make(
			{
				// left gutter
				FlexElement::Make(
					[&]() -> std::vector<std::shared_ptr<Element>> {
						return {
							// top label
							pLeftTop = TextElement::Make(L"9999", {"$body-left-top", "$axis", "$y-axis"}),
							// bottom label
							pLeftBottom = TextElement::Make(L"9999", {"$body-left-bottom", "$axis", "$y-axis"}),
						};
					}(),
					{"$body-left", "$vert-axis"}
				),
				// plot element
				pPlot = MakePlotElement(),

				// right gutter, option 1) displays a value
				pVal = TextElement::Make(L"", {"$body-right", "$value"}),
				// right gutter, option 2) displays 2nd axis
				FlexElement::Make(
					[&]() -> std::vector<std::shared_ptr<Element>> {
						return {
							// top label
							pRightTop = TextElement::Make(L"9999", {"$body-right-top", "$axis", "$y-axis"}),
							// bottom label
							pRightBottom = TextElement::Make(L"9999", {"$body-right-bottom", "$axis", "$y-axis"}),
						};
					}(),
					{"$body-right", "$vert-axis"}
				),				
			},
			{"$body"}
		));
		// bottom (x-axis) labels
		AddChild(FlexElement::Make(
			{
				FlexElement::Make({}, {"$footer-left"}),
				FlexElement::Make(
					{
						// left label
						// must preset with 0 because justification-left doesn't work like we want
						pBottomLeft = TextElement::Make(L"0", {"$footer-center-left", "$axis", "$x-axis"}),
						// right label
						pBottomRight = TextElement::Make(L"9999", {"$footer-center-right", "$axis", "$x-axis"}),
					},
					{"$footer-center"}
				),
				FlexElement::Make({}, {"$footer-right"}),
			},
			{"$footer"}
		));
	}

	GraphElement::~GraphElement() {}

	std::shared_ptr<Element> GraphElement::Make(GraphType type_, std::vector<std::shared_ptr<GraphLinePack>> packs_, std::vector<std::string> classes_)
	{
		return std::make_shared<GraphElement>(type_, std::move(packs_), std::move(classes_));
	}

	void GraphElement::SetValueRangeLeft(float min, float max)
	{
		pPlot->SetValueRangeLeft(min, max);
		// value range is x-axis for histogram, otherwise (line plot) y-axis
		if (dynamic_cast<HistogramPlotElement*>(pPlot.get()))
		{
			SetBottomAxisLabels(float(min), float(max));
		}
		else // line plot
		{
			SetAxisLabelsLeft(min, max);
		}
	}

	void GraphElement::SetValueRangeRight(float min, float max)
	{
		pPlot->SetValueRangeRight(min, max);
		SetAxisLabelsRight(min, max);
	}

	void GraphElement::SetCountRange(int min, int max)
	{
		// count range only applicable to histogram plots
		if (auto pHisto = dynamic_cast<HistogramPlotElement*>(pPlot.get()))
		{
			pHisto->SetCountRange(min, max);
			SetAxisLabelsLeft(float(min), float(max));
		}
	}

	void GraphElement::SetTimeWindow(float dt)
	{
		pPlot->SetTimeWindow(dt);
		// time range not applicable to historgram plot (for axes)
		if (!dynamic_cast<HistogramPlotElement*>(pPlot.get()))
		{
			SetBottomAxisLabels(dt, 0.f);
		}
	}

	void GraphElement::SetAxisLabelsLeft(float bottom, float top)
	{
		if (pLeftTop && pLeftBottom)
		{
			pLeftTop->SetText(std::format(L"{:.0f}", top));
			pLeftBottom->SetText(std::format(L"{:.0f}", bottom));
		}
	}

	void GraphElement::SetAxisLabelsRight(float bottom, float top)
	{
		if (pRightTop && pRightBottom)
		{
			pRightTop->SetText(std::format(L"{:.0f}", top));
			pRightBottom->SetText(std::format(L"{:.0f}", bottom));
		}
	}

	void GraphElement::SetBottomAxisLabels(float left, float right)
	{
		if (pBottomLeft && pBottomRight)
		{
			pBottomLeft->SetText(std::format(L"{:.0f}", left));
			pBottomRight->SetText(std::format(L"{:.0f}", right));
		}
	}

	void GraphElement::Draw_(Graphics& gfx) const
	{
		if (!isDualAxis) {
			auto pData = packs.front()->data;
			if (pData->Size() > 0)
			{
				if (const auto v = pData->Front().value; (!lastValue || v != *lastValue) && pVal)
				{
					pVal->SetText(std::format(L"{:.0f}", v));
					lastValue = v;
				}
			}
		}

		// auto-scaling
		if (isAutoScalingLeft || isAutoScalingRight) {
			const auto marginFactor = 0.05f;
			std::optional<float> newLeftMin;
			std::optional<float> newLeftMax;
			std::optional<float> newRightMin;
			std::optional<float> newRightMax;
			// finding largest extent over all packs for left and right axes separately
			for (const auto& p : packs) {
				if (p->axisAffinity == AxisAffinity::Left && isAutoScalingLeft) {
					if (auto min = p->data->Min()) {
						newLeftMin = std::min(newLeftMin.value_or(std::numeric_limits<float>::max()), *min);
					}
					if (auto max = p->data->Max()) {
						newLeftMax = std::max(newLeftMax.value_or(std::numeric_limits<float>::min()), *max);
					}
				}
				else if (p->axisAffinity == AxisAffinity::Right && isAutoScalingRight) {
					if (auto min = p->data->Min()) {
						newRightMin = std::min(newRightMin.value_or(std::numeric_limits<float>::max()), *min);
					}
					if (auto max = p->data->Max()) {
						newRightMax = std::max(newRightMax.value_or(std::numeric_limits<float>::min()), *max);
					}
				}
			}
			// fallback to previous value if nothing set
			newLeftMin = newLeftMin.value_or(autoLeftMin);
			newLeftMax = newLeftMax.value_or(autoLeftMax);
			newRightMin = newRightMin.value_or(autoRightMin);
			newRightMax = newRightMax.value_or(autoRightMax);
			// take action if any changes since last pass, per axis
			if (isAutoScalingLeft && (newLeftMin != autoLeftMin || newLeftMax != autoLeftMax)) {
				const auto span = *newLeftMax - *newLeftMin;
				const auto margin = std::max(span * marginFactor, 1.f);
				const auto marginedMin = *newLeftMin - margin;
				const auto marginedMax = *newLeftMax + margin;
				// TODO: figure out how to not const cast this
				const_cast<GraphElement*>(this)->SetValueRangeLeft(marginedMin, marginedMax);
				autoLeftMin = *newLeftMin;
				autoLeftMax = *newLeftMax;
			}
			if (isAutoScalingRight && (newRightMin != autoRightMin || newRightMax != autoRightMax)) {
				const auto span = *newRightMax - *newRightMin;
				const auto margin = std::max(span * marginFactor, 1.f);
				const auto marginedMin = *newRightMin - margin;
				const auto marginedMax = *newRightMax + margin;
				// TODO: figure out how to not const cast this
				const_cast<GraphElement*>(this)->SetValueRangeRight(marginedMin, marginedMax);
				autoRightMin = *newRightMin;
				autoRightMax = *newRightMax;
			}
		}

		// auto histogram counting
		if (isAutoScalingCount) {
			if (const auto pHist = dynamic_cast<HistogramPlotElement*>(pPlot.get())) {
				if (const auto max = pHist->GetMaxCount(); max != autoCountMax) {
					autoCountMax = max;
					// TODO: figure out how to not const cast this
					const_cast<GraphElement*>(this)->SetCountRange(0, int(float(autoCountMax) * 1.05f));
				}
			}
		}

		FlexElement::Draw_(gfx);
	}

	void GraphElement::SetPosition_(const Vec2& pos, const Dimensions& dimensions, sty::StyleProcessor& sp, Graphics& gfx)
	{
		SetValueRangeLeft(sp.Resolve<sty::at::graphMinValueLeft>(), sp.Resolve<sty::at::graphMaxValueLeft>());
		SetValueRangeRight(sp.Resolve<sty::at::graphMinValueRight>(), sp.Resolve<sty::at::graphMaxValueRight>());
		isAutoScalingLeft = sp.Resolve<sty::at::graphAutoscaleLeft>();
		isAutoScalingRight = sp.Resolve<sty::at::graphAutoscaleRight>();
		isAutoScalingCount = sp.Resolve<sty::at::graphAutoscaleCount>();
		SetCountRange(sp.Resolve<sty::at::graphMinCount>(), sp.Resolve<sty::at::graphMaxCount>());
		SetTimeWindow(sp.Resolve<sty::at::graphTimeWindow>());
		FlexElement::SetPosition_(pos, dimensions, sp, gfx);
	}
}