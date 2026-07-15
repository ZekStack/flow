# Configuration

`FlowConfig` controls fixed capacities and runtime transition policy.

```cpp
struct FlowConfig {
	uint16_t maxStates = 8;
	uint16_t maxTransitions = 16;
	bool threadSafe = false;
	bool allowSameState = false;
	bool allowUndefinedTransitions = false;
};
```

## `maxStates`

Maximum number of distinct states Flow may register. The initial state consumes one slot. States are also registered when they appear in transitions, callback registrations, or accepted undefined transitions.

Failed transition or callback registration is transactional and does not consume state capacity.

## `maxTransitions`

Maximum number of registered `from -> to` transition pairs. A transition path with `N` states requires `N - 1` transition slots.

## `threadSafe`

When enabled, Flow protects public access using a FreeRTOS recursive mutex. User callbacks run without that mutex held so they do not block state readers for the entire callback duration.

A second `setState()` call while another state change is executing returns `FlowStatus::Busy`. This applies both across tasks and to nested calls from callbacks.

## `allowSameState`

By default, requesting the current state returns `AlreadyInState` before transition lookup.

When enabled, a same-state request is processed like any other state change. Unless `allowUndefinedTransitions` is also enabled, a registered self-transition is required. The guard, exit callback, action, state commit, and enter callback run in the normal order. The result is `Changed`, while current and previous state both contain the same value.

## `allowUndefinedTransitions`

When disabled, only registered transitions are accepted.

When enabled, an unregistered target may be accepted and is added to the internal state table before the change. State capacity still applies. Undefined transitions have no transition guard or action, but state exit and enter callbacks still run when registered.

## Allocation behavior

Flow allocates bounded state and transition arrays during `init()`. Thread-safe mode also creates one FreeRTOS mutex.

Flow itself performs no heap allocation during `setState()`. Callback snapshots use fixed inline storage. User callback copy constructors must also avoid heap allocation when strict allocation-free state changes are required.
