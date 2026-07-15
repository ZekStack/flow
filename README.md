# Flow

Flow is a finite state machine library for Arduino ESP32 projects.

It provides explicit transitions, guards, actions, enter and exit callbacks, optional FreeRTOS mutex protection, fixed inline callback storage, and runtime diagnostics. Flow is designed for small feature-owned state machines with bounded setup-time allocation and no Flow heap allocation during `setState()`.

[![CI](https://github.com/ZekStack/flow/actions/workflows/ci.yml/badge.svg)](https://github.com/ZekStack/flow/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/ZekStack/flow?sort=semver)](https://github.com/ZekStack/flow/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE.md)

## Why use Flow?

- **Explicit state rules** — only registered transitions are allowed by default.
- **Feature-owned** — each feature can keep its own compact state machine.
- **Fixed callback storage** — capturing lambdas are stored inline with configurable capacity.
- **Predictable runtime** — Flow performs no heap allocation in `setState()`.
- **Production-minded** — result-based errors, diagnostics, optional thread safety, and no exceptions.

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

Flow is not published to Arduino Library Manager yet. Download the repository ZIP or clone it into:

```text
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
	config.threadSafe = true;

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

	if (flow.setState(FeatureState::Starting) != FlowStatus::Changed) {
		return;
	}
	flow.setState(FeatureState::Ready);
}

void loop() {
	delay(1000);
}
```

## Runtime rules

Register transitions and callbacks during setup, after `init()` and before runtime state changes.

A Flow instance permits only one active state change. Nested or concurrent `setState()` calls return `Busy`. Mutation and deinitialization APIs also return `Busy` while callbacks are running. Defer follow-up transitions through a queue, timer, Worker job, or Signal event.

Callbacks run in this order:

```text
guard -> onExit -> action -> state commit -> onEnter
```

The target state is visible through `current()` inside `onEnter`.

Callbacks must be copy constructible and move constructible. Capturing lambdas are recommended. Move-only captures are not supported because Flow snapshots callbacks before invoking them outside the mutex.

## Examples

| Example | Description |
| --- | --- |
| `Basic` | Minimal initialization, transition path, and state changes. |
| `Guards` | Guarded transitions and rejected state changes. |
| `FeatureState` | Class-owned state machine using capturing callbacks. |
| `CapturingCallbacks` | Capturing lambdas and small callable objects. |
| `Diagnostics` | Runtime counters and last transition data. |
| `ConfigAndLimits` | Capacity limits, duplicate transitions, and callback size. |
| `ReentrantBusy` | Nested state changes rejected with `Busy`. |

## Documentation

| Document | Description |
| --- | --- |
| [`docs/getting-started.md`](docs/getting-started.md) | Setup and first state flow. |
| [`docs/configuration.md`](docs/configuration.md) | Capacity, thread safety, and transition policy. |
| [`docs/callbacks.md`](docs/callbacks.md) | Callback storage, order, requirements, and restrictions. |
| [`docs/guards.md`](docs/guards.md) | Guard behavior and rejected transitions. |
| [`docs/diagnostics.md`](docs/diagnostics.md) | Counters and last-request fields. |
| [`docs/api.md`](docs/api.md) | Public types, methods, and status codes. |
| [`docs/examples.md`](docs/examples.md) | Included examples. |
| [`docs/troubleshooting.md`](docs/troubleshooting.md) | Common failures and solutions. |

## Compatibility

| Item | Support |
| --- | --- |
| Framework | Arduino ESP32 |
| Platform | `espressif32` |
| Language | C++20 |
| Filesystem | none |
| PSRAM | not used directly |
| Dependencies | none |
| Exceptions | not used |
| Version | `0.1.0` |

## License

MIT — see [`LICENSE.md`](LICENSE.md).

## ZekStack

Part of the ZekStack ESP32 library stack.
