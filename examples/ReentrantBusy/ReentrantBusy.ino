#include <Arduino.h>
#include <Flow.h>

enum class State : uint8_t {
	Idle,
	Starting,
	Ready,
};

Flow<State> flow;
FlowStatus nestedStatus = FlowStatus::Ok;

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
	config.maxStates = 3;
	config.maxTransitions = 2;

	FlowResult result = flow.init(config, State::Idle);
	if (!result) {
		Serial.println(result.message);
		return;
	}
	if (!checkStatus(flow.transition(State::Idle, State::Starting).status())) {
		return;
	}
	if (!checkStatus(flow.transition(State::Starting, State::Ready).status())) {
		return;
	}
	if (!checkStatus(
	        flow.onEnter(State::Starting, []() {
		        nestedStatus = flow.setState(State::Ready);
	        })
	    )) {
		return;
	}

	flow.setState(State::Starting);
	Serial.println(flow.statusToString(nestedStatus));
}

void loop() {
	delay(1000);
}
