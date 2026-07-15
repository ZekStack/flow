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
