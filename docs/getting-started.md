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

Initialize with bounded capacities, register transitions, then request state changes.

```cpp
FlowConfig config;
config.maxStates = 3;
config.maxTransitions = 2;

FlowResult result = flow.init(config, State::Idle);
if (!result) {
	Serial.println(result.message);
	return;
}

flow.transitionPath({State::Idle, State::Starting, State::Ready});
flow.setState(State::Starting);
flow.setState(State::Ready);
```

Register transitions and callbacks during setup. Runtime state changes should call `setState()` only.
