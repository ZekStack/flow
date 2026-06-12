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

		_flow.transition(StorageState::Idle, StorageState::Mounting)
		    .action([this]() {
			    mount();
		    });
		_flow.transition(StorageState::Mounting, StorageState::Ready)
		    .guard([this]() {
			    return _mounted;
		    });
		_flow.transition(StorageState::Mounting, StorageState::Failed);
		_flow.transition(StorageState::Failed, StorageState::Mounting);

		_flow.onEnter(StorageState::Ready, [this]() {
			onReady();
		});
		_flow.onEnter(StorageState::Failed, [this]() {
			onFailed();
		});

		return FlowResult::ok();
	}

	void start() {
		_flow.setState(StorageState::Mounting);
	}

	void handleMounted() {
		_mounted = true;
		_flow.setState(StorageState::Ready);
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
	storage.init();
	storage.start();
	storage.handleMounted();
}

void loop() {
	delay(1000);
}
