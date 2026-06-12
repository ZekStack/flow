#include <Arduino.h>
#include <Flow.h>

enum class State : uint8_t {
	Idle,
	Starting,
	Ready,
};

Flow<State> flow;
FlowStatus nestedStatus = FlowStatus::Ok;

void setup() {
	Serial.begin(115200);

	FlowConfig config;
	config.maxStates = 3;
	config.maxTransitions = 2;

	flow.init(config, State::Idle);
	flow.transition(State::Idle, State::Starting);
	flow.transition(State::Starting, State::Ready);
	flow.onEnter(State::Starting, []() {
		nestedStatus = flow.setState(State::Ready);
	});

	flow.setState(State::Starting);
	Serial.println(flow.statusToString(nestedStatus));
}

void loop() {
	delay(1000);
}
