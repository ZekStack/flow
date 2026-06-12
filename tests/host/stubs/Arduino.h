#pragma once

#include <cstdint>

extern uint32_t flowTestMillis;

inline uint32_t millis() {
	return flowTestMillis;
}
