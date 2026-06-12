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
	flow.init(config, State::Idle);
	flow.transitionPath({State::Idle, State::Starting, State::Ready});

	flow.setState(State::Starting);
	flow.setState(State::Ready);

	FlowDiag<State> diag = flow.getDiagnostics();
	Serial.print("changed=");
	Serial.println(diag.changedCount);
	Serial.print("last=");
	Serial.println(flow.statusToString(diag.lastStatus));
}

void loop() {
	delay(1000);
}
