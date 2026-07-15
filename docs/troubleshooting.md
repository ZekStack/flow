# Troubleshooting

## `NoTransition`

Register the requested `from -> to` transition before calling `setState()`, or explicitly enable `allowUndefinedTransitions`.

## `GuardRejected`

The transition exists, but its guard returned `false`. The state and callbacks after the guard remain unchanged.

## `CallbackTooLarge`

Use a smaller capture, increase the `CallbackSize` template argument, or move captured data into the owning feature object. Failed registration is rolled back.

## `Busy`

Another state change is executing, or a callback attempted to mutate or deinitialize the same Flow instance. Defer follow-up state changes through a queue, timer, Worker job, or Signal event.

With `threadSafe = true`, concurrent tasks receive `Busy` instead of entering overlapping transitions.

## `MaxStatesReached`

Increase `FlowConfig::maxStates`. The initial state, transitions, callback registrations, and accepted undefined targets all consume state slots. Failed registration does not consume a slot.

## `MaxTransitionsReached`

Increase `FlowConfig::maxTransitions`, or reduce the number of registered transition pairs.
