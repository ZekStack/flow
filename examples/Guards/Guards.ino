#include <Arduino.h>
#include <Flow.h>

enum class NetworkState : uint8_t {
	Idle,
	Connecting,
	Connected,
};

Flow<NetworkState> flow;
bool connected = false;

void setup() {
	Serial.begin(115200);

	FlowConfig config;
	config.maxStates = 3;
	config.maxTransitions = 2;

	flow.init(config, NetworkState::Idle);
	flow.transition(NetworkState::Idle, NetworkState::Connecting);
	flow.transition(NetworkState::Connecting, NetworkState::Connected)
	    .guard([]() {
		    return connected;
	    })
	    .action([]() {
		    Serial.println("connection accepted");
	    });

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
