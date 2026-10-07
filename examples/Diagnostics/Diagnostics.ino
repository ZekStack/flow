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
	config.memory.allocation = Strata::Placement::PreferExternal;
	FlowResult result = flow.init(config, State::Idle);
	if (!result) {
		Serial.println(result.message);
		return;
	}
	if (!checkStatus(flow.transitionPath({State::Idle, State::Starting, State::Ready}))) {
		return;
	}

	FlowStatus status = flow.setState(State::Starting);
	if (status != FlowStatus::Changed) {
		Serial.println(flow.statusToString(status));
		return;
	}
	status = flow.setState(State::Ready);
	if (status != FlowStatus::Changed) {
		Serial.println(flow.statusToString(status));
		return;
	}

	FlowDiag<State> diag = flow.getDiagnostics();
	Serial.print("changed=");
	Serial.println(diag.changedCount);
	Serial.print("last=");
	Serial.println(flow.statusToString(diag.lastStatus));
	Serial.print("allocation-placement=");
	Serial.println(Strata::toString(diag.allocationPlacement));
	Serial.print("state-region=");
	Serial.println(Strata::toString(diag.stateStorageRegion));
	Serial.print("transition-region=");
	Serial.println(Strata::toString(diag.transitionStorageRegion));
}

void loop() {
	delay(1000);
}
