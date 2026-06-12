#include <Flow.h>

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <new>

uint32_t flowTestMillis = 100;

namespace {
uint32_t allocationCount = 0;

enum class State : uint8_t {
	Idle,
	Starting,
	Ready,
	Failed,
	Extra,
};

void *countedAlloc(std::size_t size) {
	allocationCount++;
	return std::malloc(size);
}
} // namespace

void *operator new(std::size_t size) {
	if (void *memory = countedAlloc(size)) {
		return memory;
	}
	std::abort();
}

void *operator new[](std::size_t size) {
	if (void *memory = countedAlloc(size)) {
		return memory;
	}
	std::abort();
}

void operator delete(void *memory) noexcept {
	std::free(memory);
}

void operator delete[](void *memory) noexcept {
	std::free(memory);
}

void operator delete(void *memory, std::size_t) noexcept {
	std::free(memory);
}

void operator delete[](void *memory, std::size_t) noexcept {
	std::free(memory);
}

static void testPreInitBehavior() {
	Flow<State> flow;

	assert(!flow.initialized());
	assert(flow.current() == State::Idle);
	assert(!flow.is(State::Idle));
	assert(flow.setState(State::Ready) == FlowStatus::NotInitialized);
	assert(flow.transition(State::Idle, State::Ready).status() == FlowStatus::NotInitialized);
	assert(flow.transitionPath({State::Idle, State::Ready}) == FlowStatus::NotInitialized);
	assert(flow.onEnter(State::Ready, []() {}) == FlowStatus::NotInitialized);
	assert(flow.onExit(State::Ready, []() {}) == FlowStatus::NotInitialized);
	assert(flow.getDiagnostics().lastStatus == FlowStatus::NotInitialized);
	assert(flow.deinit());
}

static void testInitDeinitAndDoubleInit() {
	Flow<State> flow;
	FlowConfig config;
	config.maxStates = 3;
	config.maxTransitions = 3;

	FlowResult result = flow.init(config, State::Idle);
	assert(result);
	assert(flow.initialized());
	assert(flow.current() == State::Idle);
	assert(flow.init(config, State::Idle).status == FlowStatus::AlreadyInitialized);
	assert(flow.deinit());
	assert(!flow.initialized());
}

static void testInvalidConfig() {
	Flow<State> flow;
	FlowConfig config;
	config.maxStates = 0;
	assert(flow.init(config, State::Idle).status == FlowStatus::InvalidConfig);
}

static void testTransitionsAndDiagnostics() {
	Flow<State> flow;
	FlowConfig config;
	config.maxStates = 4;
	config.maxTransitions = 4;
	assert(flow.init(config, State::Idle));
	assert(flow.transitionPath({State::Idle, State::Starting, State::Ready}) == FlowStatus::Ok);
	assert(flow.transition(State::Starting, State::Failed).status() == FlowStatus::Ok);
	assert(flow.transition(State::Idle, State::Starting).status() == FlowStatus::DuplicateTransition);

	assert(flow.setState(State::Failed) == FlowStatus::NoTransition);
	assert(flow.getDiagnostics().noTransitionCount == 1);
	assert(flow.setState(State::Starting) == FlowStatus::Changed);
	assert(flow.setState(State::Starting) == FlowStatus::AlreadyInState);
	assert(flow.getDiagnostics().alreadyInStateCount == 1);
	assert(flow.setState(State::Ready) == FlowStatus::Changed);

	FlowDiag<State> diag = flow.getDiagnostics();
	assert(diag.currentState == State::Ready);
	assert(diag.previousState == State::Starting);
	assert(diag.transitionCount == 3);
	assert(diag.changedCount == 2);
	assert(diag.failedCount == 2);
	assert(diag.lastStatus == FlowStatus::Changed);
}

static void testGuardsAndCallbacks() {
	Flow<State> flow;
	FlowConfig config;
	config.maxStates = 3;
	config.maxTransitions = 2;
	assert(flow.init(config, State::Idle));

	bool ready = false;
	int sequence = 0;
	assert(
	    flow.transition(State::Idle, State::Ready)
	        .guard([&ready]() {
		        return ready;
	        })
	        .action([&sequence]() {
		        sequence = sequence * 10 + 2;
	        })
	        .status() == FlowStatus::Ok
	);
	assert(
	    flow.transition(State::Ready, State::Failed)
	        .action([&sequence]() {
		        sequence = sequence * 10 + 4;
	        })
	        .status() == FlowStatus::Ok
	);
	assert(
	    flow.onExit(State::Idle, [&sequence]() {
		    sequence = sequence * 10 + 1;
	    }) == FlowStatus::Ok
	);
	assert(
	    flow.onEnter(State::Ready, [&flow, &sequence]() {
		    assert(flow.current() == State::Ready);
		    sequence = sequence * 10 + 3;
	    }) == FlowStatus::Ok
	);
	assert(flow.onEnter(State::Ready, []() {}) == FlowStatus::CallbackAlreadyRegistered);

	assert(flow.setState(State::Ready) == FlowStatus::GuardRejected);
	assert(flow.current() == State::Idle);
	ready = true;
	assert(flow.setState(State::Ready) == FlowStatus::Changed);
	assert(sequence == 123);
	assert(flow.getDiagnostics().guardRejectedCount == 1);
}

static void testGuardDeinitReturningTrue() {
	Flow<State> flow;
	FlowConfig config;
	config.maxStates = 2;
	config.maxTransitions = 1;
	assert(flow.init(config, State::Idle));
	assert(
	    flow.transition(State::Idle, State::Ready)
	        .guard([&flow]() {
		        flow.deinit();
		        return true;
	        })
	        .status() == FlowStatus::Ok
	);

	assert(flow.setState(State::Ready) == FlowStatus::NotInitialized);
	assert(!flow.initialized());
}

static void testExitActionAndEnterDeinitBehavior() {
	{
		Flow<State> flow;
		FlowConfig config;
		config.maxStates = 2;
		config.maxTransitions = 1;
		assert(flow.init(config, State::Idle));
		assert(flow.transition(State::Idle, State::Ready).status() == FlowStatus::Ok);
		assert(
		    flow.onExit(State::Idle, [&flow]() {
			    flow.deinit();
		    }) == FlowStatus::Ok
		);

		assert(flow.setState(State::Ready) == FlowStatus::NotInitialized);
		assert(!flow.initialized());
	}

	{
		Flow<State> flow;
		FlowConfig config;
		config.maxStates = 2;
		config.maxTransitions = 1;
		assert(flow.init(config, State::Idle));
		assert(
		    flow.transition(State::Idle, State::Ready)
		        .action([&flow]() {
			        flow.deinit();
		        })
		        .status() == FlowStatus::Ok
		);

		assert(flow.setState(State::Ready) == FlowStatus::NotInitialized);
		assert(!flow.initialized());
	}

	{
		Flow<State> flow;
		FlowConfig config;
		config.maxStates = 2;
		config.maxTransitions = 1;
		assert(flow.init(config, State::Idle));
		assert(flow.transition(State::Idle, State::Ready).status() == FlowStatus::Ok);
		assert(
		    flow.onEnter(State::Ready, [&flow]() {
			    flow.deinit();
		    }) == FlowStatus::Ok
		);

		assert(flow.setState(State::Ready) == FlowStatus::Changed);
		assert(!flow.initialized());
	}
}

static void testLimitsAndCallbackSize() {
	Flow<State, 8> flow;
	FlowConfig config;
	config.maxStates = 2;
	config.maxTransitions = 1;
	assert(flow.init(config, State::Idle));
	assert(flow.transition(State::Idle, State::Starting).status() == FlowStatus::Ok);
	assert(flow.transition(State::Starting, State::Ready).status() == FlowStatus::MaxTransitionsReached);
	assert(flow.onEnter(State::Ready, []() {}) == FlowStatus::MaxStatesReached);

	struct LargeCallable {
		char data[32]{};
		void operator()() {
			data[0] = 1;
		}
	};

	Flow<State, 8> callbackFlow;
	FlowConfig callbackConfig;
	callbackConfig.maxStates = 2;
	callbackConfig.maxTransitions = 1;
	assert(callbackFlow.init(callbackConfig, State::Idle));
	assert(callbackFlow.transition(State::Idle, State::Ready).status() == FlowStatus::Ok);
	assert(callbackFlow.onEnter(State::Ready, LargeCallable{}) == FlowStatus::CallbackTooLarge);
	assert(callbackFlow.getDiagnostics().callbackTooLargeCount == 1);
	assert(
	    std::strcmp(callbackFlow.statusToString(FlowStatus::MaxCallbacksReached), "MaxCallbacksReached") ==
	    0
	);
}

static void testTransactionalTransitionPath() {
	{
		Flow<State> flow;
		FlowConfig config;
		config.maxStates = 4;
		config.maxTransitions = 2;
		assert(flow.init(config, State::Idle));

		assert(
		    flow.transitionPath({State::Idle, State::Starting, State::Ready, State::Failed}) ==
		    FlowStatus::MaxTransitionsReached
		);
		assert(flow.setState(State::Starting) == FlowStatus::NoTransition);
		assert(flow.getDiagnostics().transitionCount == 0);
	}

	{
		Flow<State> flow;
		FlowConfig config;
		config.maxStates = 3;
		config.maxTransitions = 3;
		assert(flow.init(config, State::Idle));

		assert(
		    flow.transitionPath({State::Idle, State::Starting, State::Idle, State::Ready}) ==
		    FlowStatus::Ok
		);
		assert(flow.getDiagnostics().transitionCount == 3);
		assert(flow.setState(State::Starting) == FlowStatus::Changed);
		assert(flow.setState(State::Idle) == FlowStatus::Changed);
		assert(flow.setState(State::Ready) == FlowStatus::Changed);
	}

	{
		Flow<State> flow;
		FlowConfig config;
		config.maxStates = 3;
		config.maxTransitions = 3;
		assert(flow.init(config, State::Idle));

		assert(
		    flow.transitionPath({State::Idle, State::Starting, State::Idle, State::Starting}) ==
		    FlowStatus::DuplicateTransition
		);
		assert(flow.getDiagnostics().transitionCount == 0);
		assert(flow.setState(State::Starting) == FlowStatus::NoTransition);
	}

	{
		Flow<State> flow;
		FlowConfig config;
		config.maxStates = 1;
		config.maxTransitions = 1;
		assert(flow.init(config, State::Idle));

		assert(flow.transitionPath({State::Starting, State::Idle}) == FlowStatus::MaxStatesReached);
		assert(flow.getDiagnostics().transitionCount == 0);
		assert(flow.setState(State::Starting) == FlowStatus::NoTransition);
	}
}

static void testUndefinedTransitionsRegisterTargetStates() {
	{
		Flow<State> flow;
		FlowConfig config;
		config.maxStates = 2;
		config.maxTransitions = 1;
		config.allowUndefinedTransitions = true;
		assert(flow.init(config, State::Idle));

		assert(flow.setState(State::Ready) == FlowStatus::Changed);
		assert(flow.onEnter(State::Extra, []() {}) == FlowStatus::MaxStatesReached);
	}

	{
		Flow<State> flow;
		FlowConfig config;
		config.maxStates = 1;
		config.maxTransitions = 1;
		config.allowUndefinedTransitions = true;
		assert(flow.init(config, State::Idle));

		assert(flow.setState(State::Ready) == FlowStatus::MaxStatesReached);
		FlowDiag<State> diag = flow.getDiagnostics();
		assert(diag.failedCount == 1);
		assert(diag.lastStatus == FlowStatus::MaxStatesReached);
		assert(flow.current() == State::Idle);
	}
}

static void testAllowSameState() {
	Flow<State> flow;
	FlowConfig config;
	config.maxStates = 1;
	config.maxTransitions = 1;
	config.allowSameState = true;
	assert(flow.init(config, State::Idle));
	assert(flow.transition(State::Idle, State::Idle).status() == FlowStatus::Ok);

	assert(flow.setState(State::Idle) == FlowStatus::Changed);
	assert(flow.current() == State::Idle);
}

static void testReentrantBusy() {
	Flow<State> flow;
	FlowConfig config;
	config.maxStates = 3;
	config.maxTransitions = 2;
	assert(flow.init(config, State::Idle));
	assert(flow.transition(State::Idle, State::Starting).status() == FlowStatus::Ok);
	assert(flow.transition(State::Starting, State::Ready).status() == FlowStatus::Ok);

	FlowStatus nestedStatus = FlowStatus::Ok;
	assert(
	    flow.onEnter(State::Starting, [&flow, &nestedStatus]() {
		    nestedStatus = flow.setState(State::Ready);
	    }) == FlowStatus::Ok
	);

	assert(flow.setState(State::Starting) == FlowStatus::Changed);
	assert(nestedStatus == FlowStatus::Busy);
	assert(flow.current() == State::Starting);
}

static void testNoAllocationDuringSetState() {
	Flow<State> flow;
	FlowConfig config;
	config.maxStates = 2;
	config.maxTransitions = 1;
	assert(flow.init(config, State::Idle));
	assert(
	    flow.transition(State::Idle, State::Ready)
	        .action([]() {})
	        .status() == FlowStatus::Ok
	);
	assert(flow.onExit(State::Idle, []() {}) == FlowStatus::Ok);
	assert(flow.onEnter(State::Ready, []() {}) == FlowStatus::Ok);

	allocationCount = 0;
	assert(flow.setState(State::Ready) == FlowStatus::Changed);
	assert(allocationCount == 0);
}

static void testThreadSafePathCompiles() {
	Flow<State> flow;
	FlowConfig config;
	config.threadSafe = true;
	assert(flow.init(config, State::Idle));
	assert(flow.transition(State::Idle, State::Ready).status() == FlowStatus::Ok);
	assert(flow.setState(State::Ready) == FlowStatus::Changed);
	assert(flow.deinit());
}

int main() {
	testPreInitBehavior();
	testInitDeinitAndDoubleInit();
	testInvalidConfig();
	testTransitionsAndDiagnostics();
	testGuardsAndCallbacks();
	testGuardDeinitReturningTrue();
	testExitActionAndEnterDeinitBehavior();
	testLimitsAndCallbackSize();
	testTransactionalTransitionPath();
	testUndefinedTransitionsRegisterTargetStates();
	testAllowSameState();
	testReentrantBusy();
	testNoAllocationDuringSetState();
	testThreadSafePathCompiles();
	return 0;
}
