#include <Arduino.h>
#include <Flow.h>

enum class State : uint8_t {
	Idle,
	Starting,
	Ready,
};

struct LargeCallback {
	char data[96]{};

	void operator()() {
		data[0] = 1;
	}
};

Flow<State, 32> flow;

void setup() {
	Serial.begin(115200);

	FlowConfig config;
	config.maxStates = 2;
	config.maxTransitions = 1;

	flow.init(config, State::Idle);

	Serial.println(flow.statusToString(flow.transition(State::Idle, State::Starting).status()));
	Serial.println(flow.statusToString(flow.transition(State::Idle, State::Starting).status()));
	Serial.println(flow.statusToString(flow.transition(State::Starting, State::Ready).status()));
	Serial.println(flow.statusToString(flow.onEnter(State::Starting, LargeCallback{})));
}

void loop() {
	delay(1000);
}
