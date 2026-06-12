#pragma once

#include <Arduino.h>

#include "internal/FlowFixedFunction.h"
#include "internal/FlowMutex.h"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <new>
#include <type_traits>
#include <utility>

struct FlowConfig {
	uint16_t maxStates = 8;
	uint16_t maxTransitions = 16;
	bool threadSafe = false;
	bool allowSameState = false;
	bool allowUndefinedTransitions = false;
	bool allowReentrantSetState = false;
};

enum class FlowStatus : uint8_t {
	Ok = 0,
	Changed,
	AlreadyInState,
	NoTransition,
	GuardRejected,
	NotInitialized,
	AlreadyInitialized,
	InvalidConfig,
	MaxStatesReached,
	MaxTransitionsReached,
	MaxCallbacksReached,
	DuplicateTransition,
	CallbackAlreadyRegistered,
	CallbackTooLarge,
	AllocationFailed,
	Busy,
};

struct FlowResult {
	bool success = false;
	FlowStatus status = FlowStatus::Ok;
	const char *message = nullptr;

	explicit operator bool() const {
		return success;
	}

	static FlowResult ok(const char *message = "OK") {
		return {true, FlowStatus::Ok, message};
	}

	static FlowResult error(FlowStatus status, const char *message) {
		return {false, status, message};
	}
};

template <typename State> struct FlowDiag {
	State currentState{};
	State previousState{};
	uint16_t transitionCount = 0;
	uint32_t setStateCount = 0;
	uint32_t changedCount = 0;
	uint32_t failedCount = 0;
	uint32_t noTransitionCount = 0;
	uint32_t guardRejectedCount = 0;
	uint32_t alreadyInStateCount = 0;
	uint32_t busyCount = 0;
	uint32_t callbackTooLargeCount = 0;
	FlowStatus lastStatus = FlowStatus::Ok;
	State lastRequestedState{};
	State lastFromState{};
	State lastToState{};
	uint32_t lastChangeAtMs = 0;
};

template <typename State, size_t CallbackSize = 64> class Flow;
template <typename State, size_t CallbackSize = 64> class FlowTransitionBuilder;

template <typename State, size_t CallbackSize> class FlowTransitionBuilder {
  public:
	FlowTransitionBuilder() = default;

	FlowTransitionBuilder(Flow<State, CallbackSize> *flow, uint16_t index, FlowStatus status)
	    : _flow(flow), _index(index), _status(status) {
	}

	template <typename Callback> FlowTransitionBuilder &guard(Callback callback) {
		if (_status != FlowStatus::Ok || _flow == nullptr) {
			return *this;
		}
		_status = _flow->setTransitionGuard(_index, std::move(callback));
		return *this;
	}

	template <typename Callback> FlowTransitionBuilder &action(Callback callback) {
		if (_status != FlowStatus::Ok || _flow == nullptr) {
			return *this;
		}
		_status = _flow->setTransitionAction(_index, std::move(callback));
		return *this;
	}

	FlowStatus status() const {
		return _status;
	}

	explicit operator bool() const {
		return _status == FlowStatus::Ok;
	}

	operator FlowStatus() const {
		return _status;
	}

  private:
	Flow<State, CallbackSize> *_flow = nullptr;
	uint16_t _index = 0;
	FlowStatus _status = FlowStatus::NotInitialized;
};

template <typename State, size_t CallbackSize> class Flow {
	static_assert(CallbackSize > 0, "CallbackSize must be greater than zero");
	static_assert(std::is_copy_constructible_v<State>, "State must be copy constructible");

  public:
	using Callback = FlowFixedFunction<void(), CallbackSize>;
	using GuardCallback = FlowFixedFunction<bool(), CallbackSize>;
	using Builder = FlowTransitionBuilder<State, CallbackSize>;

	Flow() = default;

	~Flow() {
		deinit();
	}

	Flow(const Flow &) = delete;
	Flow &operator=(const Flow &) = delete;

	FlowResult init(const FlowConfig &config, State initialState) {
		if (_initialized) {
			return FlowResult::error(
			    FlowStatus::AlreadyInitialized,
			    "flow is already initialized"
			);
		}
		if (config.maxStates == 0 || config.maxTransitions == 0) {
			return FlowResult::error(FlowStatus::InvalidConfig, "flow config is invalid");
		}

		if (config.threadSafe && !_mutex.create()) {
			return FlowResult::error(FlowStatus::AllocationFailed, "flow mutex allocation failed");
		}

		_states = new (std::nothrow) StateEntry[config.maxStates];
		_transitions = new (std::nothrow) TransitionEntry[config.maxTransitions];
		if (_states == nullptr || _transitions == nullptr) {
			delete[] _states;
			delete[] _transitions;
			_states = nullptr;
			_transitions = nullptr;
			_mutex.destroy();
			return FlowResult::error(FlowStatus::AllocationFailed, "flow storage allocation failed");
		}

		_config = config;
		_current = initialState;
		_previous = initialState;
		_stateCount = 0;
		_transitionCount = 0;
		_busy = false;
		_initialized = true;
		_diag = FlowDiag<State>{};
		_diag.currentState = initialState;
		_diag.previousState = initialState;
		_diag.lastRequestedState = initialState;
		_diag.lastFromState = initialState;
		_diag.lastToState = initialState;
		_diag.lastStatus = FlowStatus::Ok;

		const FlowStatus status = ensureState(initialState);
		if (status != FlowStatus::Ok) {
			deinit();
			return FlowResult::error(status, "flow initial state registration failed");
		}

		return FlowResult::ok();
	}

	FlowResult deinit() {
		const bool useMutex = _config.threadSafe;
		{
			FlowLock lock(_mutex, useMutex);
			if (!lock) {
				return FlowResult::error(FlowStatus::Busy, "flow mutex lock failed");
			}

			delete[] _states;
			delete[] _transitions;
			_states = nullptr;
			_transitions = nullptr;
			_stateCount = 0;
			_transitionCount = 0;
			_busy = false;
			_initialized = false;
			_config = FlowConfig{};
			_current = State{};
			_previous = State{};
			_diag = FlowDiag<State>{};
		}
		if (useMutex) {
			_mutex.destroy();
		}
		return FlowResult::ok();
	}

	bool initialized() const {
		FlowLock lock(const_cast<FlowMutex &>(_mutex), _config.threadSafe);
		return lock && _initialized;
	}

	State current() const {
		FlowLock lock(const_cast<FlowMutex &>(_mutex), _config.threadSafe);
		if (!lock || !_initialized) {
			return State{};
		}
		return _current;
	}

	bool is(State state) const {
		FlowLock lock(const_cast<FlowMutex &>(_mutex), _config.threadSafe);
		return lock && _initialized && _current == state;
	}

	FlowStatus setState(State state) {
		State from{};
		GuardCallback guardCallback;
		Callback exitCallback;
		Callback actionCallback;
		Callback enterCallback;
		bool hasGuard = false;
		bool hasExit = false;
		bool hasAction = false;
		bool hasEnter = false;

		{
			FlowLock lock(_mutex, _config.threadSafe);
			if (!lock) {
				return finishFailedRequest(state, State{}, State{}, FlowStatus::Busy);
			}

			if (!_initialized) {
				FlowDiag<State> diag{};
				diag.lastStatus = FlowStatus::NotInitialized;
				_diag = diag;
				return FlowStatus::NotInitialized;
			}

			_diag.setStateCount++;
			_diag.lastRequestedState = state;
			from = _current;
			_diag.lastFromState = from;
			_diag.lastToState = state;

			if (_busy && !_config.allowReentrantSetState) {
				return recordFailureLocked(FlowStatus::Busy);
			}

			if (from == state && !_config.allowSameState) {
				return recordFailureLocked(FlowStatus::AlreadyInState);
			}

			TransitionEntry *transition = findTransition(from, state);
			if (transition == nullptr && !_config.allowUndefinedTransitions) {
				return recordFailureLocked(FlowStatus::NoTransition);
			}
			if (transition == nullptr && _config.allowUndefinedTransitions) {
				const FlowStatus status = ensureState(state);
				if (status != FlowStatus::Ok) {
					return recordFailureLocked(status);
				}
			}

			_busy = true;

			_previous = from;
			StateEntry *fromEntry = findState(from);
			StateEntry *toEntry = findState(state);
			if (transition != nullptr && transition->hasGuard) {
				guardCallback = transition->guard;
				hasGuard = true;
			}
			if (fromEntry != nullptr && fromEntry->hasOnExit) {
				exitCallback = fromEntry->onExit;
				hasExit = true;
			}
			if (transition != nullptr && transition->hasAction) {
				actionCallback = transition->action;
				hasAction = true;
			}
			if (toEntry != nullptr && toEntry->hasOnEnter) {
				enterCallback = toEntry->onEnter;
				hasEnter = true;
			}
		}

		bool guardAccepted = true;
		if (hasGuard) {
			guardAccepted = guardCallback();
		}

		{
			FlowLock lock(_mutex, _config.threadSafe);
			if (!lock) {
				return FlowStatus::Busy;
			}
			if (!_initialized || !_busy) {
				return FlowStatus::NotInitialized;
			}
			if (!guardAccepted) {
				_busy = false;
				return recordFailureLocked(FlowStatus::GuardRejected);
			}
		}

		if (hasExit) {
			exitCallback();
		}
		if (hasAction) {
			actionCallback();
		}

		{
			FlowLock lock(_mutex, _config.threadSafe);
			if (!lock) {
				return FlowStatus::Busy;
			}
			if (!_initialized || !_busy) {
				return FlowStatus::NotInitialized;
			}
			_current = state;
			_diag.currentState = state;
			_diag.previousState = from;
			_diag.changedCount++;
			_diag.lastStatus = FlowStatus::Changed;
			_diag.lastChangeAtMs = millis();
		}

		if (hasEnter) {
			enterCallback();
		}

		{
			FlowLock lock(_mutex, _config.threadSafe);
			if (!lock) {
				return FlowStatus::Busy;
			}
			if (!_initialized) {
				return FlowStatus::Changed;
			}
			_busy = false;
		}

		return FlowStatus::Changed;
	}

	Builder transition(State from, State to) {
		FlowLock lock(_mutex, _config.threadSafe);
		if (!lock) {
			return Builder(this, 0, FlowStatus::Busy);
		}
		if (!_initialized) {
			return Builder(this, 0, FlowStatus::NotInitialized);
		}
		if (findTransition(from, to) != nullptr) {
			return Builder(this, 0, FlowStatus::DuplicateTransition);
		}
		if (_transitionCount >= _config.maxTransitions) {
			return Builder(this, 0, FlowStatus::MaxTransitionsReached);
		}

		FlowStatus status = ensureState(from);
		if (status != FlowStatus::Ok) {
			return Builder(this, 0, status);
		}
		status = ensureState(to);
		if (status != FlowStatus::Ok) {
			return Builder(this, 0, status);
		}

		const uint16_t index = _transitionCount++;
		_transitions[index].from = from;
		_transitions[index].to = to;
		_transitions[index].hasGuard = false;
		_transitions[index].hasAction = false;
		_diag.transitionCount = _transitionCount;
		return Builder(this, index, FlowStatus::Ok);
	}

	FlowStatus transitionPath(std::initializer_list<State> states) {
		FlowLock lock(_mutex, _config.threadSafe);
		if (!lock) {
			return FlowStatus::Busy;
		}
		if (!_initialized) {
			return FlowStatus::NotInitialized;
		}
		if (states.size() < 2) {
			return FlowStatus::Ok;
		}

		const size_t transitionsToAdd = states.size() - 1;
		const size_t availableTransitions = _config.maxTransitions - _transitionCount;
		if (transitionsToAdd > availableTransitions) {
			return FlowStatus::MaxTransitionsReached;
		}

		const size_t availableStates = _config.maxStates - _stateCount;
		size_t newStateCount = 0;
		auto iterator = states.begin();
		State previous = *iterator;
		if (!stateExistsInPathPrefixOrFlow(previous, states.begin(), iterator)) {
			newStateCount++;
			if (newStateCount > availableStates) {
				return FlowStatus::MaxStatesReached;
			}
		}
		++iterator;
		for (; iterator != states.end(); ++iterator) {
			if (findTransition(previous, *iterator) != nullptr ||
			    transitionExistsInPathPrefix(previous, *iterator, states.begin(), iterator)) {
				return FlowStatus::DuplicateTransition;
			}
			if (!stateExistsInPathPrefixOrFlow(*iterator, states.begin(), iterator)) {
				newStateCount++;
				if (newStateCount > availableStates) {
					return FlowStatus::MaxStatesReached;
				}
			}
			previous = *iterator;
		}

		iterator = states.begin();
		previous = *iterator;
		ensureState(previous);
		++iterator;
		for (; iterator != states.end(); ++iterator) {
			ensureState(*iterator);
			const uint16_t index = _transitionCount++;
			_transitions[index].from = previous;
			_transitions[index].to = *iterator;
			_transitions[index].hasGuard = false;
			_transitions[index].hasAction = false;
			previous = *iterator;
		}
		_diag.transitionCount = _transitionCount;
		return FlowStatus::Ok;
	}

	template <typename CallbackType> FlowStatus onEnter(State state, CallbackType callback) {
		return setStateCallback(state, std::move(callback), true);
	}

	template <typename CallbackType> FlowStatus onExit(State state, CallbackType callback) {
		return setStateCallback(state, std::move(callback), false);
	}

	FlowDiag<State> getDiagnostics() const {
		FlowLock lock(const_cast<FlowMutex &>(_mutex), _config.threadSafe);
		if (!lock) {
			FlowDiag<State> diag{};
			diag.lastStatus = FlowStatus::Busy;
			return diag;
		}
		if (!_initialized) {
			FlowDiag<State> diag{};
			diag.lastStatus = FlowStatus::NotInitialized;
			return diag;
		}
		return _diag;
	}

	const char *statusToString(FlowStatus status) const {
		switch (status) {
		case FlowStatus::Ok:
			return "Ok";
		case FlowStatus::Changed:
			return "Changed";
		case FlowStatus::AlreadyInState:
			return "AlreadyInState";
		case FlowStatus::NoTransition:
			return "NoTransition";
		case FlowStatus::GuardRejected:
			return "GuardRejected";
		case FlowStatus::NotInitialized:
			return "NotInitialized";
		case FlowStatus::AlreadyInitialized:
			return "AlreadyInitialized";
		case FlowStatus::InvalidConfig:
			return "InvalidConfig";
		case FlowStatus::MaxStatesReached:
			return "MaxStatesReached";
		case FlowStatus::MaxTransitionsReached:
			return "MaxTransitionsReached";
		case FlowStatus::MaxCallbacksReached:
			return "MaxCallbacksReached";
		case FlowStatus::DuplicateTransition:
			return "DuplicateTransition";
		case FlowStatus::CallbackAlreadyRegistered:
			return "CallbackAlreadyRegistered";
		case FlowStatus::CallbackTooLarge:
			return "CallbackTooLarge";
		case FlowStatus::AllocationFailed:
			return "AllocationFailed";
		case FlowStatus::Busy:
			return "Busy";
		default:
			return "Unknown";
		}
	}

  private:
	friend class FlowTransitionBuilder<State, CallbackSize>;

	struct StateEntry {
		State state{};
		Callback onEnter;
		Callback onExit;
		bool hasOnEnter = false;
		bool hasOnExit = false;
	};

	struct TransitionEntry {
		State from{};
		State to{};
		GuardCallback guard;
		Callback action;
		bool hasGuard = false;
		bool hasAction = false;
	};

	template <typename CallbackType>
	FlowStatus setTransitionGuard(uint16_t index, CallbackType callback) {
		FlowLock lock(_mutex, _config.threadSafe);
		if (!lock) {
			return FlowStatus::Busy;
		}
		if (!_initialized || index >= _transitionCount) {
			return FlowStatus::NotInitialized;
		}
		if (!_transitions[index].guard.assign(std::move(callback))) {
			_diag.callbackTooLargeCount++;
			return FlowStatus::CallbackTooLarge;
		}
		_transitions[index].hasGuard = true;
		return FlowStatus::Ok;
	}

	template <typename CallbackType>
	FlowStatus setTransitionAction(uint16_t index, CallbackType callback) {
		FlowLock lock(_mutex, _config.threadSafe);
		if (!lock) {
			return FlowStatus::Busy;
		}
		if (!_initialized || index >= _transitionCount) {
			return FlowStatus::NotInitialized;
		}
		if (!_transitions[index].action.assign(std::move(callback))) {
			_diag.callbackTooLargeCount++;
			return FlowStatus::CallbackTooLarge;
		}
		_transitions[index].hasAction = true;
		return FlowStatus::Ok;
	}

	template <typename CallbackType>
	FlowStatus setStateCallback(State state, CallbackType callback, bool enter) {
		FlowLock lock(_mutex, _config.threadSafe);
		if (!lock) {
			return FlowStatus::Busy;
		}
		if (!_initialized) {
			return FlowStatus::NotInitialized;
		}

		FlowStatus status = ensureState(state);
		if (status != FlowStatus::Ok) {
			return status;
		}

		StateEntry *entry = findState(state);
		if (entry == nullptr) {
			return FlowStatus::MaxStatesReached;
		}

		if (enter) {
			if (entry->hasOnEnter) {
				return FlowStatus::CallbackAlreadyRegistered;
			}
			if (!entry->onEnter.assign(std::move(callback))) {
				_diag.callbackTooLargeCount++;
				return FlowStatus::CallbackTooLarge;
			}
			entry->hasOnEnter = true;
			return FlowStatus::Ok;
		}

		if (entry->hasOnExit) {
			return FlowStatus::CallbackAlreadyRegistered;
		}
		if (!entry->onExit.assign(std::move(callback))) {
			_diag.callbackTooLargeCount++;
			return FlowStatus::CallbackTooLarge;
		}
		entry->hasOnExit = true;
		return FlowStatus::Ok;
	}

	StateEntry *findState(State state) {
		for (uint16_t index = 0; index < _stateCount; index++) {
			if (_states[index].state == state) {
				return &_states[index];
			}
		}
		return nullptr;
	}

	const StateEntry *findState(State state) const {
		for (uint16_t index = 0; index < _stateCount; index++) {
			if (_states[index].state == state) {
				return &_states[index];
			}
		}
		return nullptr;
	}

	TransitionEntry *findTransition(State from, State to) {
		for (uint16_t index = 0; index < _transitionCount; index++) {
			if (_transitions[index].from == from && _transitions[index].to == to) {
				return &_transitions[index];
			}
		}
		return nullptr;
	}

	bool stateExistsInPathPrefixOrFlow(
	    State state,
	    const State *begin,
	    const State *current
	) const {
		if (findState(state) != nullptr) {
			return true;
		}
		for (const State *iterator = begin; iterator != current; ++iterator) {
			if (*iterator == state) {
				return true;
			}
		}
		return false;
	}

	bool transitionExistsInPathPrefix(
	    State from,
	    State to,
	    const State *begin,
	    const State *current
	) const {
		if (begin == current) {
			return false;
		}
		const State *previous = begin;
		const State *iterator = begin;
		++iterator;
		for (; iterator != current; ++iterator) {
			if (*previous == from && *iterator == to) {
				return true;
			}
			previous = iterator;
		}
		return false;
	}

	FlowStatus ensureState(State state) {
		if (findState(state) != nullptr) {
			return FlowStatus::Ok;
		}
		if (_stateCount >= _config.maxStates) {
			return FlowStatus::MaxStatesReached;
		}
		_states[_stateCount].state = state;
		_states[_stateCount].hasOnEnter = false;
		_states[_stateCount].hasOnExit = false;
		_stateCount++;
		return FlowStatus::Ok;
	}

	FlowStatus recordFailureLocked(FlowStatus status) {
		_diag.lastStatus = status;
		_diag.failedCount++;
		if (status == FlowStatus::NoTransition) {
			_diag.noTransitionCount++;
		} else if (status == FlowStatus::GuardRejected) {
			_diag.guardRejectedCount++;
		} else if (status == FlowStatus::AlreadyInState) {
			_diag.alreadyInStateCount++;
		} else if (status == FlowStatus::Busy) {
			_diag.busyCount++;
		}
		return status;
	}

	FlowStatus finishFailedRequest(State requested, State from, State to, FlowStatus status) {
		FlowLock lock(_mutex, _config.threadSafe);
		if (lock && _initialized) {
			_diag.setStateCount++;
			_diag.lastRequestedState = requested;
			_diag.lastFromState = from;
			_diag.lastToState = to;
			return recordFailureLocked(status);
		}
		return status;
	}

	FlowConfig _config{};
	FlowMutex _mutex;
	StateEntry *_states = nullptr;
	TransitionEntry *_transitions = nullptr;
	uint16_t _stateCount = 0;
	uint16_t _transitionCount = 0;
	State _current{};
	State _previous{};
	bool _initialized = false;
	bool _busy = false;
	FlowDiag<State> _diag{};
};
