#pragma once

#include "FreeRTOS.h"

#include <mutex>
#include <new>

struct FlowTestRecursiveMutex {
	std::recursive_mutex mutex;
};

using SemaphoreHandle_t = FlowTestRecursiveMutex *;

inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutex() {
	return new (std::nothrow) FlowTestRecursiveMutex();
}

inline void vSemaphoreDelete(SemaphoreHandle_t handle) {
	delete handle;
}

inline int xSemaphoreTakeRecursive(SemaphoreHandle_t handle, TickType_t) {
	if (handle == nullptr) {
		return pdFALSE;
	}
	handle->mutex.lock();
	return pdTRUE;
}

inline void xSemaphoreGiveRecursive(SemaphoreHandle_t handle) {
	if (handle != nullptr) {
		handle->mutex.unlock();
	}
}
