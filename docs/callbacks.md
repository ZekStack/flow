# Callbacks

Flow supports transition guards, transition actions, state exit callbacks, and state enter callbacks.

Callbacks use fixed inline storage:

```cpp
Flow<State> flow;      // 64 bytes per callback
Flow<State, 96> large; // 96 bytes per callback
```

If a callback does not fit, registration returns `FlowStatus::CallbackTooLarge`. Failed callback registration is transactional and does not leave a new state or transition behind.

## Callable requirements

Flow callbacks must be copy constructible and move constructible because Flow snapshots them before releasing its internal mutex. Capturing lambdas and small callable objects are recommended.

Move-only captures such as a lambda owning a `std::unique_ptr` are not supported. User callback copy constructors should not allocate when allocation-free `setState()` behavior matters.

`std::function` is not used for production callback storage. `std::bind` may work when the generated callable fits into `CallbackSize`, but it is not the preferred style.

## Callback order

A successful state change executes:

```text
guard -> onExit -> action -> state commit -> onEnter
```

If the guard returns `false`, exit, action, commit, and enter do not run. `current()` already reports the target state inside `onEnter`.

## Restrictions during a state change

The following operations return `Busy` while callbacks for the same Flow instance are running:

- `setState()`
- `transition()`
- `transitionPath()`
- `onEnter()`
- `onExit()`
- `deinit()`

Defer follow-up state changes through a queue, timer, Worker job, or Signal event. Read-only calls such as `current()`, `is()`, and `getDiagnostics()` remain available.

One `onEnter` and one `onExit` callback are supported per state in v0.1.0. Registering the same slot twice returns `CallbackAlreadyRegistered`.
