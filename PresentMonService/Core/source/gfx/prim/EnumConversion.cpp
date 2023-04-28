// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "EnumConversion.h"
#include <Core/source/infra/log/Logging.h>
#include <format>


namespace p2c::gfx::prim
{
	DWRITE_FONT_WEIGHT ConvertWeight(Weight w)
	{
		using enum Weight;
		switch (w)
		{
		case Normal: return DWRITE_FONT_WEIGHT_NORMAL;
		case Bold: return DWRITE_FONT_WEIGHT_BOLD;
		case Light: return DWRITE_FONT_WEIGHT_LIGHT;
		case Medium: return DWRITE_FONT_WEIGHT_MEDIUM;
		case ExtraBold: return DWRITE_FONT_WEIGHT_EXTRA_BOLD;
		case UltraLight: return DWRITE_FONT_WEIGHT_ULTRA_LIGHT;
		default:
			p2clog.warn(std::format(L"Bad weight: {}", (int)w)).commit();
			return DWRITE_FONT_WEIGHT_NORMAL;
		}
	}

	DWRITE_FONT_STYLE ConvertStyle(Style s)
	{
		using enum Style;
		switch (s)
		{
		case Normal: return DWRITE_FONT_STYLE_NORMAL;
		case Oblique: return DWRITE_FONT_STYLE_OBLIQUE;
		case Italic: return DWRITE_FONT_STYLE_ITALIC;
		default:
			p2clog.warn(std::format(L"Bad style: {}", (int)s)).commit();
			return DWRITE_FONT_STYLE_NORMAL;
		}
	}

	DWRITE_PARAGRAPH_ALIGNMENT ConvertAlignment(Alignment a)
	{
		using enum Alignment;
		switch (a)
		{
		case Center: return DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
		case Top: return DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
		case Bottom: return DWRITE_PARAGRAPH_ALIGNMENT_FAR;
		default:
			p2clog.warn(std::format(L"Bad algnment: {}", (int)a)).commit();
			return DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
		}
	}

	DWRITE_TEXT_ALIGNMENT ConvertJustification(Justification j)
	{
		using enum Justification;
		switch (j)
		{
		case Center: return DWRITE_TEXT_ALIGNMENT_CENTER;
		case Left: return DWRITE_TEXT_ALIGNMENT_LEADING;
		case Right: return DWRITE_TEXT_ALIGNMENT_TRAILING;
		case Full: return DWRITE_TEXT_ALIGNMENT_JUSTIFIED;
		default:
			p2clog.warn(std::format(L"Bad justification: {}", (int)j)).commit();
			return DWRITE_TEXT_ALIGNMENT_LEADING;
		}
	}
}