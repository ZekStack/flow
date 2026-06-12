#pragma once

#include <Arduino.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class FlowMutex {
  public:
	FlowMutex() = default;

	~FlowMutex() {
		destroy();
	}

	FlowMutex(const FlowMutex &) = delete;
	FlowMutex &operator=(const FlowMutex &) = delete;

	bool create() {
		if (_handle != nullptr) {
			return true;
		}
		_handle = xSemaphoreCreateRecursiveMutex();
		return _handle != nullptr;
	}

	void destroy() {
		if (_handle != nullptr) {
			vSemaphoreDelete(_handle);
			_handle = nullptr;
		}
	}

	bool lock(TickType_t timeout = portMAX_DELAY) {
		return _handle != nullptr && xSemaphoreTakeRecursive(_handle, timeout) == pdTRUE;
	}

	void unlock() {
		if (_handle != nullptr) {
			xSemaphoreGiveRecursive(_handle);
		}
	}

  private:
	SemaphoreHandle_t _handle = nullptr;
};

class FlowLock {
  public:
	FlowLock(FlowMutex &mutex, bool enabled) : _mutex(mutex), _enabled(enabled) {
		if (_enabled) {
			_locked = _mutex.lock();
		} else {
			_locked = true;
		}
	}

	~FlowLock() {
		if (_enabled && _locked) {
			_mutex.unlock();
		}
	}

	FlowLock(const FlowLock &) = delete;
	FlowLock &operator=(const FlowLock &) = delete;

	explicit operator bool() const {
		return _locked;
	}

  private:
	FlowMutex &_mutex;
	bool _enabled = false;
	bool _locked = false;
};
