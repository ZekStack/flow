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

`transition()` returns a temporary builder. `FlowTransitionBuilder` is intended for immediate chained use directly from `transition()`. Do not store builders across other Flow mutations.

If transition creation fails, later `guard()` and `action()` calls are no-ops and preserve the original failure status. If `guard()` or `action()` fails with `CallbackTooLarge`, Flow rolls back the transition created by that builder. This prevents a transition intended to be guarded or action-backed from remaining registered without the failed callback.

`transitionPath()` is transactional. If any transition in the path cannot be registered, no transitions from that call are added.

`MaxCallbacksReached` is reserved for future multi-callback support and is not emitted by v0.0.1 production code.
