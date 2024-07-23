#pragma once
#include <cstdint>

enum class AcceptDecision : uint32_t {
	// i.e. the top bit determines if this is accepted, and the rest determine how many characters to consume
	Accept = 0x8000'0000,
	ConsumeMask = 0x7FFF'FFFF
};