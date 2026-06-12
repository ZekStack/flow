#include <Arduino.h>
#include <Flow.h>

enum class StorageState : uint8_t {
	Idle,
	Mounting,
	Ready,
	Failed,
};

class StorageFeature {
  public:
	FlowResult init() {
		FlowConfig config;
		config.maxStates = 4;
		config.maxTransitions = 4;
		config.threadSafe = true;

		FlowResult result = _flow.init(config, StorageState::Idle);
		if (!result) {
			return result;
		}

		FlowStatus status = _flow.transition(StorageState::Idle, StorageState::Mounting)
		                        .action([this]() {
			                        mount();
		                        })
		                        .status();
		if (status != FlowStatus::Ok) {
			return FlowResult::error(status, _flow.statusToString(status));
		}

		status = _flow.transition(StorageState::Mounting, StorageState::Ready)
		             .guard([this]() {
			             return _mounted;
		             })
		             .status();
		if (status != FlowStatus::Ok) {
			return FlowResult::error(status, _flow.statusToString(status));
		}

		status = _flow.transition(StorageState::Mounting, StorageState::Failed).status();
		if (status != FlowStatus::Ok) {
			return FlowResult::error(status, _flow.statusToString(status));
		}

		status = _flow.transition(StorageState::Failed, StorageState::Mounting).status();
		if (status != FlowStatus::Ok) {
			return FlowResult::error(status, _flow.statusToString(status));
		}

		status = _flow.onEnter(StorageState::Ready, [this]() {
			onReady();
		});
		if (status != FlowStatus::Ok) {
			return FlowResult::error(status, _flow.statusToString(status));
		}

		status = _flow.onEnter(StorageState::Failed, [this]() {
			onFailed();
		});
		if (status != FlowStatus::Ok) {
			return FlowResult::error(status, _flow.statusToString(status));
		}

		return FlowResult::ok();
	}

	FlowStatus start() {
		return _flow.setState(StorageState::Mounting);
	}

	FlowStatus handleMounted() {
		_mounted = true;
		return _flow.setState(StorageState::Ready);
	}

	const char *statusToString(FlowStatus status) const {
		return _flow.statusToString(status);
	}

	bool ready() const {
		return _flow.is(StorageState::Ready);
	}

  private:
	void mount() {
		Serial.println("mounting storage");
	}

	void onReady() {
		Serial.println("storage ready");
	}

	void onFailed() {
		Serial.println("storage failed");
	}

	bool _mounted = false;
	Flow<StorageState> _flow;
};

StorageFeature storage;

void setup() {
	Serial.begin(115200);
	FlowResult result = storage.init();
	if (!result) {
		Serial.println(result.message);
		return;
	}
	FlowStatus status = storage.start();
	if (status != FlowStatus::Changed) {
		Serial.println(storage.statusToString(status));
		return;
	}
	status = storage.handleMounted();
	if (status != FlowStatus::Changed) {
		Serial.println(storage.statusToString(status));
		return;
	}
}

void loop() {
	delay(1000);
}
