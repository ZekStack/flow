#include <Arduino.h>
#include <Flow.h>

enum class State : uint8_t {
	Idle,
	Starting,
	Ready,
};

Flow<State> flow;

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

	flow.transitionPath({State::Idle, State::Starting, State::Ready});
	flow.onEnter(State::Ready, []() {
		Serial.println("ready");
	});

	flow.setState(State::Starting);
	flow.setState(State::Ready);
}

void loop() {
	delay(1000);
}
