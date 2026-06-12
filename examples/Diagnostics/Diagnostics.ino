#include <Arduino.h>
#include <Flow.h>

enum class State : uint8_t {
	Idle,
	Starting,
	Ready,
};

Flow<State> flow;

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
	if (!checkStatus(flow.transitionPath({State::Idle, State::Starting, State::Ready}))) {
		return;
	}

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
