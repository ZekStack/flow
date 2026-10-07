#pragma once

#include <Arduino.h>
#include <strata/freertos/Mutex.h>

class FlowMutex {
  public:
	FlowMutex() = default;

	FlowMutex(const FlowMutex &) = delete;
	FlowMutex &operator=(const FlowMutex &) = delete;

	bool create() {
		if (_mutex) {
			return true;
		}
		_mutex = Strata::FreeRTOS::RecursiveMutex::create();
		return static_cast<bool>(_mutex);
	}

	void destroy() {
		_mutex.reset();
	}

	bool lock(TickType_t timeout = portMAX_DELAY) {
		return _mutex.lock(timeout);
	}

	void unlock() {
		_mutex.unlock();
	}

	Strata::Region controlRegion() const {
		return _mutex.controlRegion();
	}

  private:
	Strata::FreeRTOS::RecursiveMutex _mutex;
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
