#pragma once
#include <cstdint>

enum class AcceptDecision : uint32_t {
	// i.e. the top bit determines if this is accepted, and the rest determine how many characters to consume
	Accept = 0x8000'0000,
	ConsumeMask = 0x7FFF'FFFF // Note: we just apply this mask and run it, but it doesn't include the sign bit so
								// if we want to return negative values we need to manually sign extend
								// We also cannot safely return values with absolute value > 1, as we might skip
								// over the null byte
};