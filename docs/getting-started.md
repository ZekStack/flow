# Getting started

Include Flow and define an enum for the states owned by one feature.

```cpp
#include <Flow.h>

enum class State : uint8_t {
	Idle,
	Starting,
	Ready,
};

Flow<State> flow;
```

Initialize fixed capacities and the initial state:

```cpp
FlowConfig config;
config.maxStates = 3;
config.maxTransitions = 2;
config.threadSafe = true;
config.memory.allocation = Strata::Placement::PreferExternal;

FlowResult result = flow.init(config, State::Idle);
if (!result) {
	Serial.println(result.message);
	return;
}
```

Register transitions and callbacks during setup:

```cpp
FlowStatus status = flow.transitionPath({State::Idle, State::Starting, State::Ready});
if (status != FlowStatus::Ok) {
	Serial.println(flow.statusToString(status));
	return;
}

status = flow.onEnter(State::Ready, []() {
	Serial.println("ready");
});
```

Runtime state changes use `setState()`:

```cpp
if (flow.setState(State::Starting) != FlowStatus::Changed) {
	return;
}
flow.setState(State::Ready);
```

Do not mutate or deinitialize the same Flow instance from one of its callbacks. Such calls return `Busy`; defer them through the application event system.


## Memory placement

Flow `v0.2.0` uses Strata `v0.1.4`. Use `config.memory.allocation` to select the placement of Flow's bounded state and transition storage. Leave it at `Default` to preserve backend-default behavior, use `PreferExternal` when internal fallback is acceptable, or use `RequireExternal` when initialization must fail without external memory.

Flow owns no task stack. Thread-safe mutex control storage remains internal automatically.
