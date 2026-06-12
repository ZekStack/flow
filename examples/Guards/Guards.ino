#include <Arduino.h>
#include <Flow.h>

enum class NetworkState : uint8_t {
	Idle,
	Connecting,
	Connected,
};

Flow<NetworkState> flow;
bool connected = false;

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

	FlowResult result = flow.init(config, NetworkState::Idle);
	if (!result) {
		Serial.println(result.message);
		return;
	}
	if (!checkStatus(flow.transition(NetworkState::Idle, NetworkState::Connecting).status())) {
		return;
	}
	if (!checkStatus(
	        flow.transition(NetworkState::Connecting, NetworkState::Connected)
	            .guard([]() {
		            return connected;
	            })
	            .action([]() {
		            Serial.println("connection accepted");
	            })
	            .status()
	    )) {
		return;
	}

	flow.setState(NetworkState::Connecting);
	FlowStatus status = flow.setState(NetworkState::Connected);
	Serial.println(flow.statusToString(status));

	connected = true;
	status = flow.setState(NetworkState::Connected);
	Serial.println(flow.statusToString(status));
}

void loop() {
	delay(1000);
}
