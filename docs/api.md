# API

## Main type

```cpp
template<typename State, size_t CallbackSize = 64>
class Flow;
```

`State` must be default constructible, copy constructible, and equality comparable by the operations used in Flow.

## Methods

```cpp
FlowResult init(const FlowConfig& config, State initialState);
FlowResult deinit();
bool initialized() const;

State current() const;
bool is(State state) const;

FlowStatus setState(State state);

FlowTransitionBuilder<State, CallbackSize> transition(State from, State to);
FlowStatus transitionPath(std::initializer_list<State> states);

FlowStatus onEnter(State state, Callback callback);
FlowStatus onExit(State state, Callback callback);

FlowDiag<State> getDiagnostics() const;
const char* statusToString(FlowStatus status) const;
```

## Status codes

```cpp
Ok
Changed
AlreadyInState
NoTransition
GuardRejected
NotInitialized
AlreadyInitialized
InvalidConfig
MaxStatesReached
MaxTransitionsReached
MaxCallbacksReached
DuplicateTransition
CallbackAlreadyRegistered
CallbackTooLarge
AllocationFailed
Busy
```

Calling `init()` twice returns `AlreadyInitialized`. Calling `deinit()` before initialization returns `Ok`. Calling `deinit()` during a state change returns `Busy`.

`transition()` returns a temporary builder intended for immediate chained use. Do not store builders across other Flow mutations.

If transition creation fails, later `guard()` and `action()` calls are no-ops and preserve the original failure. If guard or action registration fails, the new transition and any states introduced only by it are rolled back.

`transitionPath()` is transactional. If any transition or state in the path cannot be registered, nothing from that call is added.

`onEnter()` and `onExit()` are also transactional when registering a previously unknown state. An oversized callback does not consume state capacity.

`MaxCallbacksReached` is reserved for future multi-callback support and is not emitted by v0.1.0 production code.
