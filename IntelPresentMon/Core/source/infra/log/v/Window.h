#pragma once

namespace p2c::infra::log::v
{
	// verbose logging module for the system that tracks overlay target process ancestry and
	// windows spawning therein
#ifndef VVV_WINDOW
	inline constexpr bool window = false;
#else
	inline constexpr bool window = true;
#endif
}