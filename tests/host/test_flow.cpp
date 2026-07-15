#include <Flow.h>

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <new>
#include <thread>

uint32_t flowTestMillis = 100;

namespace {
std::atomic<uint32_t> allocationCount{0};

enum class State : uint8_t {
	Idle,
	Starting,
	Ready,
	Failed,
	Extra,
};

void *countedAlloc(std::size_t size) {
	allocationCount.fetch_add(1, std::memory_order_relaxed);
	return std::malloc(size);
}

struct LargeCallback {
	char data[32]{};
	void operator()() {
		data[0] = 1;
	}
};

struct LargeGuard {
	char data[32]{};
	bool operator()() {
		return data[0] == 0;
	}
};

struct LifetimeCallback {
	int *live = nullptr;
	int *calls = nullptr;

	LifetimeCallback(int &liveCount, int &callCount) : live(&liveCount), calls(&callCount) {
		(*live)++;
	}

	LifetimeCallback(const LifetimeCallback &other) : live(other.live), calls(other.calls) {
		(*live)++;
	}

	LifetimeCallback(LifetimeCallback &&other) noexcept : live(other.live), calls(other.calls) {
		(*live)++;
	}

	~LifetimeCallback() {
		if (live != nullptr) {
			(*live)--;
		}
	}

	void operator()() {
		(*calls)++;
	}
};
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

void *operator new(std::size_t size, const std::nothrow_t &) noexcept {
	return countedAlloc(size);
}

void *operator new[](std::size_t size, const std::nothrow_t &) noexcept {
	return countedAlloc(size);
}

void operator delete(void *memory, const std::nothrow_t &) noexcept {
	std::free(memory);
}

void operator delete[](void *memory, const std::nothrow_t &) noexcept {
	std::free(memory);
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

static void testInitDeinitAndInvalidConfig() {
	Flow<State> flow;
	FlowConfig config;
	config.maxStates = 3;
	config.maxTransitions = 3;

	assert(flow.init(config, State::Idle));
	assert(flow.initialized());
	assert(flow.current() == State::Idle);
	assert(flow.init(config, State::Idle).status == FlowStatus::AlreadyInitialized);
	assert(flow.deinit());
	assert(!flow.initialized());

	config.maxStates = 0;
	assert(flow.init(config, State::Idle).status == FlowStatus::InvalidConfig);
}

static void testTransitionsCallbacksAndDiagnostics() {
	Flow<State> flow;
	FlowConfig config;
	config.maxStates = 4;
	config.maxTransitions = 4;
	assert(flow.init(config, State::Idle));
	assert(flow.transitionPath({State::Idle, State::Starting, State::Ready}) == FlowStatus::Ok);
	assert(flow.transition(State::Starting, State::Failed).status() == FlowStatus::Ok);
	assert(flow.transition(State::Idle, State::Starting).status() == FlowStatus::DuplicateTransition);

	assert(flow.setState(State::Failed) == FlowStatus::NoTransition);
	assert(flow.setState(State::Starting) == FlowStatus::Changed);
	assert(flow.setState(State::Starting) == FlowStatus::AlreadyInState);
	flowTestMillis = 250;
	assert(flow.setState(State::Ready) == FlowStatus::Changed);

	const FlowDiag<State> diag = flow.getDiagnostics();
	assert(diag.currentState == State::Ready);
	assert(diag.previousState == State::Starting);
	assert(diag.transitionCount == 3);
	assert(diag.changedCount == 2);
	assert(diag.failedCount == 2);
	assert(diag.noTransitionCount == 1);
	assert(diag.alreadyInStateCount == 1);
	assert(diag.lastStatus == FlowStatus::Changed);
	assert(diag.lastChangeAtMs == 250);
}

static void testGuardAndCallbackOrder() {
	Flow<State> flow;
	FlowConfig config;
	config.maxStates = 2;
	config.maxTransitions = 1;
	assert(flow.init(config, State::Idle));

	bool ready = false;
	int sequence = 0;
	assert(
	    flow.transition(State::Idle, State::Ready)
	        .guard([&]() {
		        sequence = sequence * 10 + 1;
		        return ready;
	        })
	        .action([&]() {
		        sequence = sequence * 10 + 3;
	        })
	        .status() == FlowStatus::Ok
	);
	assert(
	    flow.onExit(State::Idle, [&]() {
		    sequence = sequence * 10 + 2;
	    }) == FlowStatus::Ok
	);
	assert(
	    flow.onEnter(State::Ready, [&]() {
		    assert(flow.current() == State::Ready);
		    sequence = sequence * 10 + 4;
	    }) == FlowStatus::Ok
	);
	assert(flow.onEnter(State::Ready, []() {}) == FlowStatus::CallbackAlreadyRegistered);

	assert(flow.setState(State::Ready) == FlowStatus::GuardRejected);
	assert(sequence == 1);
	assert(flow.current() == State::Idle);

	sequence = 0;
	ready = true;
	assert(flow.setState(State::Ready) == FlowStatus::Changed);
	assert(sequence == 1234);
	assert(flow.getDiagnostics().guardRejectedCount == 1);
}

static void testNestedSetStateAlwaysBusy() {
	{
		Flow<State> flow;
		FlowConfig config;
		config.maxStates = 3;
		config.maxTransitions = 2;
		assert(flow.init(config, State::Idle));
		assert(flow.transition(State::Idle, State::Starting).status() == FlowStatus::Ok);
		assert(flow.transition(State::Starting, State::Ready).status() == FlowStatus::Ok);
		FlowStatus nested = FlowStatus::Ok;
		assert(
		    flow.transition(State::Idle, State::Starting)
		        .status() == FlowStatus::DuplicateTransition
		);
		assert(
		    flow.onEnter(State::Starting, [&]() {
			    nested = flow.setState(State::Ready);
		    }) == FlowStatus::Ok
		);
		assert(flow.setState(State::Starting) == FlowStatus::Changed);
		assert(nested == FlowStatus::Busy);
	}

	{
		Flow<State> flow;
		FlowConfig config;
		config.maxStates = 3;
		config.maxTransitions = 2;
		assert(flow.init(config, State::Idle));
		assert(flow.transition(State::Idle, State::Starting).status() == FlowStatus::Ok);
		assert(flow.transition(State::Idle, State::Ready).status() == FlowStatus::Ok);
		FlowStatus nested = FlowStatus::Ok;
		assert(
		    flow.onExit(State::Idle, [&]() {
			    nested = flow.setState(State::Ready);
		    }) == FlowStatus::Ok
		);
		assert(flow.setState(State::Starting) == FlowStatus::Changed);
		assert(nested == FlowStatus::Busy);
	}

	{
		Flow<State> flow;
		FlowConfig config;
		config.maxStates = 3;
		config.maxTransitions = 2;
		assert(flow.init(config, State::Idle));
		FlowStatus nested = FlowStatus::Ok;
		assert(
		    flow.transition(State::Idle, State::Starting)
		        .action([&]() {
			        nested = flow.setState(State::Ready);
		        })
		        .status() == FlowStatus::Ok
		);
		assert(flow.transition(State::Idle, State::Ready).status() == FlowStatus::Ok);
		assert(flow.setState(State::Starting) == FlowStatus::Changed);
		assert(nested == FlowStatus::Busy);
	}

	{
		Flow<State> flow;
		FlowConfig config;
		config.maxStates = 3;
		config.maxTransitions = 2;
		assert(flow.init(config, State::Idle));
		FlowStatus nested = FlowStatus::Ok;
		assert(
		    flow.transition(State::Idle, State::Starting)
		        .guard([&]() {
			        nested = flow.setState(State::Ready);
			        return true;
		        })
		        .status() == FlowStatus::Ok
		);
		assert(flow.transition(State::Idle, State::Ready).status() == FlowStatus::Ok);
		assert(flow.setState(State::Starting) == FlowStatus::Changed);
		assert(nested == FlowStatus::Busy);
	}
}

static void testMutationAndDeinitRejectedDuringTransition() {
	Flow<State> flow;
	FlowConfig config;
	config.maxStates = 5;
	config.maxTransitions = 4;
	assert(flow.init(config, State::Idle));

	FlowStatus transitionStatus = FlowStatus::Ok;
	FlowStatus pathStatus = FlowStatus::Ok;
	FlowStatus enterStatus = FlowStatus::Ok;
	FlowStatus exitStatus = FlowStatus::Ok;
	FlowResult deinitResult;

	assert(
	    flow.transition(State::Idle, State::Starting)
	        .action([&]() {
		        transitionStatus = flow.transition(State::Starting, State::Ready).status();
		        pathStatus = flow.transitionPath({State::Starting, State::Ready});
		        enterStatus = flow.onEnter(State::Ready, []() {});
		        exitStatus = flow.onExit(State::Starting, []() {});
		        deinitResult = flow.deinit();
	        })
	        .status() == FlowStatus::Ok
	);

	assert(flow.setState(State::Starting) == FlowStatus::Changed);
	assert(transitionStatus == FlowStatus::Busy);
	assert(pathStatus == FlowStatus::Busy);
	assert(enterStatus == FlowStatus::Busy);
	assert(exitStatus == FlowStatus::Busy);
	assert(!deinitResult);
	assert(deinitResult.status == FlowStatus::Busy);
	assert(flow.initialized());
	assert(flow.current() == State::Starting);
}

static void testLimitsAndTransactionalRegistration() {
	Flow<State, 8> flow;
	FlowConfig config;
	config.maxStates = 2;
	config.maxTransitions = 1;
	assert(flow.init(config, State::Idle));
	assert(flow.transition(State::Idle, State::Starting).status() == FlowStatus::Ok);
	assert(flow.transition(State::Starting, State::Ready).status() == FlowStatus::MaxTransitionsReached);
	assert(flow.onEnter(State::Ready, []() {}) == FlowStatus::MaxStatesReached);

	{
		Flow<State, 8> callbackFlow;
		assert(callbackFlow.init(config, State::Idle));
		assert(callbackFlow.onEnter(State::Ready, LargeCallback{}) == FlowStatus::CallbackTooLarge);
		assert(callbackFlow.onEnter(State::Starting, []() {}) == FlowStatus::Ok);
		assert(callbackFlow.getDiagnostics().callbackTooLargeCount == 1);
	}

	{
		Flow<State, 8> callbackFlow;
		assert(callbackFlow.init(config, State::Idle));
		assert(callbackFlow.onExit(State::Ready, LargeCallback{}) == FlowStatus::CallbackTooLarge);
		assert(callbackFlow.onExit(State::Starting, []() {}) == FlowStatus::Ok);
	}

	{
		Flow<State, 8> callbackFlow;
		assert(callbackFlow.init(config, State::Idle));
		assert(callbackFlow.transition(State::Idle, State::Ready).status() == FlowStatus::Ok);
		assert(callbackFlow.onEnter(State::Ready, LargeCallback{}) == FlowStatus::CallbackTooLarge);
		assert(callbackFlow.setState(State::Ready) == FlowStatus::Changed);
	}
}

static void testBuilderFailureRollsBackTransitionAndCallable() {
	Flow<State, 8> flow;
	FlowConfig config;
	config.maxStates = 2;
	config.maxTransitions = 1;
	assert(flow.init(config, State::Idle));

	assert(
	    flow.transition(State::Idle, State::Ready)
	        .guard(LargeGuard{})
	        .status() == FlowStatus::CallbackTooLarge
	);
	assert(flow.getDiagnostics().transitionCount == 0);
	assert(flow.setState(State::Ready) == FlowStatus::NoTransition);
	assert(flow.onEnter(State::Starting, []() {}) == FlowStatus::Ok);

	assert(flow.deinit());

	Flow<State, 32> lifetimeFlow;
	assert(lifetimeFlow.init(config, State::Idle));
	int live = 0;
	int calls = 0;
	{
		auto builder = lifetimeFlow.transition(State::Idle, State::Ready);
		assert(
		    builder.guard([&]() { return true; })
		            .action(LifetimeCallback{live, calls})
		            .status() == FlowStatus::Ok
		);
		assert(live > 0);
		assert(lifetimeFlow.setState(State::Ready) == FlowStatus::Changed);
		assert(calls == 1);
	}
	assert(lifetimeFlow.deinit());
	assert(live == 0);
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
		assert(flow.getDiagnostics().transitionCount == 0);
		assert(flow.setState(State::Starting) == FlowStatus::NoTransition);
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
	}
}

static void testUndefinedAndSameStateTransitions() {
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
		config.allowSameState = true;
		assert(flow.init(config, State::Idle));
		assert(flow.transition(State::Idle, State::Idle).status() == FlowStatus::Ok);
		assert(flow.setState(State::Idle) == FlowStatus::Changed);
		assert(flow.current() == State::Idle);
	}
}

static void testNoFlowAllocationDuringSetState() {
	Flow<State> flow;
	FlowConfig config;
	config.maxStates = 2;
	config.maxTransitions = 1;
	assert(flow.init(config, State::Idle));
	assert(flow.transition(State::Idle, State::Ready).action([]() {}).status() == FlowStatus::Ok);
	assert(flow.onExit(State::Idle, []() {}) == FlowStatus::Ok);
	assert(flow.onEnter(State::Ready, []() {}) == FlowStatus::Ok);

	allocationCount.store(0, std::memory_order_relaxed);
	assert(flow.setState(State::Ready) == FlowStatus::Changed);
	assert(allocationCount.load(std::memory_order_relaxed) == 0);
}

static void testThreadSafeContention() {
	Flow<State> flow;
	FlowConfig config;
	config.maxStates = 3;
	config.maxTransitions = 2;
	config.threadSafe = true;
	assert(flow.init(config, State::Idle));

	std::atomic<bool> actionEntered{false};
	std::atomic<bool> releaseAction{false};
	assert(
	    flow.transition(State::Idle, State::Starting)
	        .action([&]() {
		        actionEntered.store(true, std::memory_order_release);
		        while (!releaseAction.load(std::memory_order_acquire)) {
			        std::this_thread::yield();
		        }
	        })
	        .status() == FlowStatus::Ok
	);
	assert(flow.transition(State::Idle, State::Ready).status() == FlowStatus::Ok);

	FlowStatus firstStatus = FlowStatus::Ok;
	std::thread worker([&]() {
		firstStatus = flow.setState(State::Starting);
	});

	while (!actionEntered.load(std::memory_order_acquire)) {
		std::this_thread::yield();
	}

	const FlowStatus competingStatus = flow.setState(State::Ready);
	assert(competingStatus == FlowStatus::Busy);
	assert(flow.current() == State::Idle);
	releaseAction.store(true, std::memory_order_release);
	worker.join();

	assert(firstStatus == FlowStatus::Changed);
	assert(flow.current() == State::Starting);
	assert(flow.getDiagnostics().busyCount == 1);
	assert(flow.deinit());
}

static void testStatusStrings() {
	Flow<State> flow;
	assert(std::strcmp(flow.statusToString(FlowStatus::Busy), "Busy") == 0);
	assert(std::strcmp(flow.statusToString(FlowStatus::MaxCallbacksReached), "MaxCallbacksReached") == 0);
}

int main() {
	testPreInitBehavior();
	testInitDeinitAndInvalidConfig();
	testTransitionsCallbacksAndDiagnostics();
	testGuardAndCallbackOrder();
	testNestedSetStateAlwaysBusy();
	testMutationAndDeinitRejectedDuringTransition();
	testLimitsAndTransactionalRegistration();
	testBuilderFailureRollsBackTransitionAndCallable();
	testTransactionalTransitionPath();
	testUndefinedAndSameStateTransitions();
	testNoFlowAllocationDuringSetState();
	testThreadSafeContention();
	testStatusStrings();
	return 0;
}
