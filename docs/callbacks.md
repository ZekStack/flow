# Callbacks

Flow supports transition actions, enter callbacks, exit callbacks, and guards.

Callbacks use fixed inline storage:

```cpp
Flow<State> flow;      // 64 bytes per callback
Flow<State, 96> large; // 96 bytes per callback
```

If a callback does not fit, registration returns `FlowStatus::CallbackTooLarge`.

Capturing lambdas are the recommended callback style:

```cpp
flow.onEnter(State::Ready, [this]() {
	handleReady();
});
```

One `onEnter` and one `onExit` callback are supported per state in v0.0.1. Registering the same slot twice returns `FlowStatus::CallbackAlreadyRegistered`.

`std::function` is not used for production callback storage. `std::bind` may work when the generated callable fits into `CallbackSize`, but it is not the preferred style.

Callbacks should not call `deinit()`, `init()`, `transition()`, `transitionPath()`, `onEnter()`, or `onExit()` on the same Flow instance during `setState()`.

If a guard, exit callback, or action deinitializes Flow before the state commit, `setState()` returns `FlowStatus::NotInitialized`. If `onEnter` deinitializes Flow after the state commit, `setState()` returns `FlowStatus::Changed`.
