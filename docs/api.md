# API

## Main type

```cpp
template<typename State, size_t CallbackSize = 64>
class Flow;
```

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

`FlowStatus` values are:

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

Calling `init()` twice returns `AlreadyInitialized`. Calling `deinit()` before init returns `Ok`.

`transition()` returns a temporary builder. If transition creation fails, later `guard()` and `action()` calls are no-ops and preserve the original failure status.
