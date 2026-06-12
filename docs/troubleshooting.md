# Troubleshooting

## `NoTransition`

Register the requested `from -> to` transition before calling `setState()`.

## `GuardRejected`

The transition exists, but its guard returned `false`.

## `CallbackTooLarge`

Use a smaller capture, increase `CallbackSize`, or move captured data into the owning feature object.

## `Busy`

A callback tried to call `setState()` while another state change was already running. Defer the follow-up state change through a driver callback, timer, Worker job, or Signal event.

## `MaxStatesReached`

Increase `FlowConfig::maxStates`. States are registered when they appear in transitions or callbacks.
