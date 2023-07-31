// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "RectBorderPrimitive.h"
#include <Core/source/win/WinAPI.h>
#include <d2d1_3.h>
#include "../base/InternalGeometry.h"
#include <array>
#include <Core/source/infra/log/Logging.h>


namespace p2c::gfx::prim
{
	RectBorderPrimitive::RectBorderPrimitive(const Rect& rect, const Skirt& stroke, std::shared_ptr<BrushPrimitive> pBrushPrim_, Graphics& gfx)
		:
		pBrushPrim{ std::move(pBrushPrim_) },
		pBrush{ Brush(*pBrushPrim) }
	{
		{
			ComPtr<ID2D1RectangleGeometry> top, bottom, left, right;
			p2chrlog << Factory(gfx).CreateRectangleGeometry({ rect.left, rect.top, rect.right, rect.top + stroke.top }, &top);
			p2chrlog << Factory(gfx).CreateRectangleGeometry({ rect.left, rect.bottom - stroke.bottom, rect.right, rect.bottom }, &bottom);
			p2chrlog << Factory(gfx).CreateRectangleGeometry({ rect.left, rect.top + stroke.top, rect.left + stroke.left, rect.bottom - stroke.bottom }, &left);
			p2chrlog << Factory(gfx).CreateRectangleGeometry({ rect.right - stroke.right, rect.top + stroke.top, rect.right, rect.bottom - stroke.bottom }, &right);
			ID2D1Geometry* components[]{ top.Get(), bottom.Get(), left.Get(), right.Get() };
			p2chrlog << Factory(gfx).CreateGeometryGroup(D2D1_FILL_MODE::D2D1_FILL_MODE_ALTERNATE, components, (UINT32)std::size(components), &pGeometry);
		}
		p2chrlog << Context(gfx).CreateFilledGeometryRealization(pGeometry.Get(), D2D1_DEFAULT_FLATTENING_TOLERANCE, &pRealization);
	}

	RectBorderPrimitive::~RectBorderPrimitive() {}

	void RectBorderPrimitive::Draw(Graphics& gfx) const
	{
		Context(gfx).DrawGeometryRealization(pRealization.Get(), pBrush.Get());
	}
}