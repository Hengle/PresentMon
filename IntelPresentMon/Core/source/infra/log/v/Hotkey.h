#pragma once

namespace p2c::infra::log::v
{
	// verbose logging module for the system that processes raw keyboard input
	// to generate hotkey events
#ifndef VVV_HOTKEY
	inline constexpr bool hotkey = false;
#else
	inline constexpr bool hotkey = true;
#endif
}