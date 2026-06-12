#include <Arduino.h>
#include <Flow.h>

enum class State : uint8_t {
	Idle,
	Ready,
};

struct ReadyPrinter {
	const char *message = nullptr;

	void operator()() {
		Serial.println(message);
	}
};

Flow<State> flow;
uint32_t readyCount = 0;

bool checkStatus(FlowStatus status) {
	if (status == FlowStatus::Ok) {
		return true;
	}
	Serial.println(flow.statusToString(status));
	return false;
}

void setup() {
	Serial.begin(115200);

	FlowConfig config;
	config.maxStates = 2;
	config.maxTransitions = 1;

	FlowResult result = flow.init(config, State::Idle);
	if (!result) {
		Serial.println(result.message);
		return;
	}
	if (!checkStatus(
	        flow.transition(State::Idle, State::Ready)
	            .action([]() {
		            readyCount++;
	            })
	            .status()
	    )) {
		return;
	}
	if (!checkStatus(flow.onEnter(State::Ready, ReadyPrinter{"small callable object"}))) {
		return;
	}

	flow.setState(State::Ready);
	Serial.println(readyCount);
}

void loop() {
	delay(1000);
}
