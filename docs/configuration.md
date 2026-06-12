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

No heap allocation is performed by `setState()`, guards, actions, `onEnter()`, or `onExit()`.

When `threadSafe` is enabled, Flow protects public state access with a FreeRTOS recursive mutex. User callbacks run without the mutex held.
