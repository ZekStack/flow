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

void setup() {
	Serial.begin(115200);

	FlowConfig config;
	config.maxStates = 2;
	config.maxTransitions = 1;

	flow.init(config, State::Idle);
	flow.transition(State::Idle, State::Ready)
	    .action([]() {
		    readyCount++;
	    });
	flow.onEnter(State::Ready, ReadyPrinter{"small callable object"});

	flow.setState(State::Ready);
	Serial.println(readyCount);
}

void loop() {
	delay(1000);
}
