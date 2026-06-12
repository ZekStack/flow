# Configuration

`FlowConfig` controls storage limits and runtime behavior.

```cpp
struct FlowConfig {
	uint16_t maxStates = 8;
	uint16_t maxTransitions = 16;
	bool threadSafe = false;
	bool allowSameState = false;
	bool allowUndefinedTransitions = false;
	bool allowReentrantSetState = false;
};
```

Flow allocates bounded state and transition storage during `init()` only. If allocation fails, `init()` returns `FlowStatus::AllocationFailed`.

Flow performs no heap allocation during `setState()`. User callable copy constructors should also avoid heap allocation.

When `threadSafe` is enabled, Flow protects public state access with a FreeRTOS recursive mutex. User callbacks run without the mutex held.

When `allowUndefinedTransitions` is enabled, accepted target states are registered in Flow's internal state table before the state changes.
