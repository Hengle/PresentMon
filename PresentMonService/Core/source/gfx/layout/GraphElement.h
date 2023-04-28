// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include "FlexElement.h"
#include "PlotElement.h"


namespace p2c::gfx::lay
{
	class TextElement;
	class LinePlotElement;
	struct GraphLinePack;

	class GraphElement : public FlexElement
	{
	public:
		// functions
		GraphElement(GraphType type_, std::vector<std::shared_ptr<GraphLinePack>> packs_, std::vector<std::string> classes_ = {});
		~GraphElement() override;
		static std::shared_ptr<Element> Make(GraphType type, std::vector<std::shared_ptr<GraphLinePack>> packs, std::vector<std::string> classes = {});
		void SetValueRangeLeft(float min, float max);
		void SetValueRangeRight(float min, float max);
		void SetTimeWindow(float dt);
		void SetCountRange(int min, int max);
	protected:
		void Draw_(Graphics& gfx) const override;
		void SetPosition_(const Vec2& pos, const Dimensions& dimensions, sty::StyleProcessor& sp, Graphics& gfx) override;
	private:
		// types
		class VirtualLineElement;
		// functions
		void SetAxisLabelsLeft(float bottom, float top);
		void SetAxisLabelsRight(float bottom, float top);
		void SetBottomAxisLabels(float left, float right);
		// data
		std::shared_ptr<PlotElement> pPlot;
		std::shared_ptr<TextElement> pVal;
		std::shared_ptr<TextElement> pLeftTop;
		std::shared_ptr<TextElement> pLeftBottom;
		std::shared_ptr<TextElement> pRightTop;
		std::shared_ptr<TextElement> pRightBottom;
		std::shared_ptr<TextElement> pBottomLeft;
		std::shared_ptr<TextElement> pBottomRight;
		std::vector<std::shared_ptr<GraphLinePack>> packs;
		bool isDualAxis = false;
		bool isAutoScalingLeft = false;
		bool isAutoScalingRight = false;
		bool isAutoScalingCount = true;
		mutable std::optional<float> lastValue;
		mutable float autoLeftMin = 0.f;
		mutable float autoLeftMax = 100.f;
		mutable float autoRightMin = 0.f;
		mutable float autoRightMax = 100.f;
		mutable int autoCountMax = 1000;
	};
}