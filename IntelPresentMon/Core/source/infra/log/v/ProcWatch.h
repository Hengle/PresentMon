#pragma once

namespace p2c::infra::log::v
{
	// verbose logging module for the system that tracks overlay target process ancestry and
	// windows spawning therein
#ifndef VVV_PROCWATCH
	inline constexpr bool procwatch = false;
#else
	inline constexpr bool procwatch = true;
#endif
}