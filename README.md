# Flow

Flow is a finite state machine library for ESP32.

Flow helps features manage internal state in Arduino ESP32 projects with explicit transitions, guards, actions, enter callbacks, exit callbacks, optional FreeRTOS mutex protection, and diagnostics. It is designed for feature-owned state machines with predictable setup and no Flow heap allocation during state changes.

[![CI](https://github.com/ZekStack/flow/actions/workflows/ci.yml/badge.svg)](https://github.com/ZekStack/flow/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/ZekStack/flow?sort=semver)](https://github.com/ZekStack/flow/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE.md)

## Why use Flow?

* **Explicit state rules** - only registered transitions are allowed by default.
* **Feature-owned** - each feature can keep its own small state machine.
* **Fixed callback storage** - capturing lambdas are stored inline with a configurable size.
* **Predictable runtime** - Flow performs no heap allocation in `setState()`.
* **Production-minded** - result-based errors, diagnostics, optional thread safety, and no exceptions.

## Install

### PlatformIO

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino

lib_deps =
  https://github.com/ZekStack/flow.git

build_flags =
  -std=gnu++20
build_unflags =
  -std=gnu++11
```

### Arduino IDE

Flow is not published to Arduino Library Manager yet.

Install it by downloading the repository ZIP or cloning it into your Arduino libraries folder.

```txt
Arduino/libraries/Flow
```

## Quick start

```cpp
#include <Arduino.h>
#include <Flow.h>

enum class FeatureState : uint8_t {
	Idle,
	Starting,
	Ready,
	Failed,
};

Flow<FeatureState> flow;

void setup() {
	Serial.begin(115200);

	FlowConfig config;
	config.maxStates = 4;
	config.maxTransitions = 4;

	FlowResult result = flow.init(config, FeatureState::Idle);
	if (!result) {
		Serial.println(result.message);
		return;
	}

	FlowStatus status = flow.transitionPath({
	    FeatureState::Idle,
	    FeatureState::Starting,
	    FeatureState::Ready,
	});
	if (status != FlowStatus::Ok) {
		Serial.println(flow.statusToString(status));
		return;
	}

	status = flow.transition(FeatureState::Starting, FeatureState::Failed).status();
	if (status != FlowStatus::Ok) {
		Serial.println(flow.statusToString(status));
		return;
	}

	status = flow.onEnter(FeatureState::Ready, []() {
		Serial.println("ready");
	});
	if (status != FlowStatus::Ok) {
		Serial.println(flow.statusToString(status));
		return;
	}

	flow.setState(FeatureState::Starting);
	flow.setState(FeatureState::Ready);
}

void loop() {
	delay(1000);
}
```

## Important notes

> [!IMPORTANT]
> Register transitions and callbacks during setup, after `init()` and before runtime state changes. Flow performs no heap allocation in `setState()`, and user callbacks run without the internal mutex held.

* Flow is for internal feature state.
* Phase is for async application lifecycle orchestration.
* Signal is for communication between features.
* `current()` before `init()` returns a default-initialized state; use `initialized()` when validity matters.
* One `onEnter` and one `onExit` callback are supported per state in v0.0.1.
* `std::function` is not used for production callback storage.
* Capturing lambdas are the recommended callback style. `std::bind` may work only when the generated callable fits into `CallbackSize`.
* User callable copy constructors should avoid heap allocation.
* Callbacks should not call `deinit()`, `init()`, `transition()`, `transitionPath()`, `onEnter()`, or `onExit()` on the same Flow instance during `setState()`.
* If a guard, exit callback, or action deinitializes Flow before the state commit, `setState()` returns `NotInitialized`. If `onEnter` deinitializes Flow after the commit, `setState()` returns `Changed`.

## Examples

| Example | Description |
| --- | --- |
| `Basic` | Minimal init, transition path, and state changes. |
| `Guards` | Guarded transitions and rejected state changes. |
| `FeatureState` | Class-owned state machine using capturing callbacks. |
| `CapturingCallbacks` | Capturing lambdas and small callable objects. |
| `Diagnostics` | Runtime counters and last transition data. |
| `ConfigAndLimits` | Capacity limits, duplicate transitions, and callback size. |
| `ReentrantBusy` | Nested state changes rejected with `Busy`. |

Start with:

```txt
examples/Basic
```

## Documentation

Detailed documentation is available in the `docs/` folder.

| Document | Description |
| --- | --- |
| [`docs/getting-started.md`](docs/getting-started.md) | Step-by-step setup and first state flow. |
| [`docs/configuration.md`](docs/configuration.md) | Config options and capacity rules. |
| [`docs/callbacks.md`](docs/callbacks.md) | Callback storage, enter/exit callbacks, and actions. |
| [`docs/guards.md`](docs/guards.md) | Guard behavior and rejected transitions. |
| [`docs/diagnostics.md`](docs/diagnostics.md) | Counters and last request fields. |
| [`docs/api.md`](docs/api.md) | Public types, methods, and status codes. |
| [`docs/examples.md`](docs/examples.md) | Explanation of all included examples. |
| [`docs/troubleshooting.md`](docs/troubleshooting.md) | Common issues and solutions. |

## API overview

```cpp
Flow<State> flow;
flow.init(config, State::Idle);
flow.transition(State::Idle, State::Ready)
    .guard([]() { return true; })
    .action([]() {});
flow.onEnter(State::Ready, []() {});
flow.setState(State::Ready);

FlowDiag<State> diag = flow.getDiagnostics();
flow.deinit();
```

For the full API, see [`docs/api.md`](docs/api.md).

## Compatibility

| Item | Support |
| --- | --- |
| Framework | Arduino ESP32 |
| Platform | `espressif32` |
| Language | C++20 |
| Filesystem | none |
| PSRAM | not used directly |
| Dependencies | none |
| Exceptions | Not used |
| Status | Early-stage `0.0.1` |

## Configuration

```cpp
FlowConfig config;
config.maxStates = 8;
config.maxTransitions = 16;
config.threadSafe = true;

FlowResult result = flow.init(config, State::Idle);
```

For all options, see [`docs/configuration.md`](docs/configuration.md).

## Error handling

Flow reports initialization through `FlowResult` and operations through `FlowStatus`.

```cpp
FlowStatus status = flow.setState(State::Ready);

if (status != FlowStatus::Changed) {
	Serial.println(flow.statusToString(status));
}
```

For all status codes, see [`docs/api.md`](docs/api.md).

## License

MIT - see [`LICENSE.md`](LICENSE.md).

## ZekStack

Part of the ZekStack ESP32 library stack.
