#pragma once
#include <cstdint>

//TODO: This probably will need to add Ignore for optionals, as they can fail to match and not advance the cursor
enum class AcceptDecision : int32_t {
	Consume, // Not a match, but can try again with a later substring
	Accept, // Match
	Reject // Not a match
};