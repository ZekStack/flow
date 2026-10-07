#include <Flow.h>

#include <cstdio>

uint32_t flowTestMillis = 100;

namespace {
int failureCount = 0;

enum class State : uint8_t {
	Idle,
	Ready,
};

void expect(bool condition, const char *message) {
	if (condition) {
		return;
	}
	std::printf("FAIL: %s\n", message);
	failureCount++;
}

void testDefaultMemoryPolicy() {
	Flow<State> flow;
	FlowConfig config;
	expect(static_cast<bool>(flow.init(config, State::Idle)), "default memory policy initializes");
	FlowDiag<State> diag = flow.getDiagnostics();
	expect(
	    diag.allocationPlacement == Strata::Placement::Default,
	    "default diagnostics report default allocation placement"
	);
	expect(static_cast<bool>(flow.deinit()), "default memory policy deinitializes");
}

void testPlacementCanChangeAcrossReinit() {
	Flow<State> flow;

	FlowConfig internalConfig;
	internalConfig.memory.allocation = Strata::Placement::Internal;
	expect(static_cast<bool>(flow.init(internalConfig, State::Idle)), "internal allocation initializes");
	expect(
	    flow.getDiagnostics().allocationPlacement == Strata::Placement::Internal,
	    "internal placement is retained in diagnostics"
	);
	expect(static_cast<bool>(flow.deinit()), "internal allocation deinitializes");

	FlowConfig preferredExternalConfig;
	preferredExternalConfig.memory.allocation = Strata::Placement::PreferExternal;
	expect(
	    static_cast<bool>(flow.init(preferredExternalConfig, State::Idle)),
	    "preferred external allocation reinitializes"
	);
	expect(
	    flow.getDiagnostics().allocationPlacement == Strata::Placement::PreferExternal,
	    "preferred external placement is retained in diagnostics"
	);
	expect(static_cast<bool>(flow.deinit()), "preferred external allocation deinitializes");
}

void testUnsupportedRequiredExternalIsTransactional() {
	Flow<State> flow;
	FlowConfig config;
	config.memory.allocation = Strata::Placement::RequireExternal;
	FlowResult result = flow.init(config, State::Idle);
	expect(
	    !result && result.status == FlowStatus::AllocationFailed,
	    "required external failure reports allocation failure"
	);
	expect(!flow.initialized(), "failed storage allocation leaves Flow uninitialized");

	FlowConfig fallback;
	fallback.memory.allocation = Strata::Placement::Internal;
	expect(static_cast<bool>(flow.init(fallback, State::Idle)), "Flow initializes after transactional allocation failure");
	expect(static_cast<bool>(flow.deinit()), "Flow deinitializes after transactional allocation failure");
}

void testInvalidMemoryPolicyIsRejected() {
	Flow<State> flow;
	FlowConfig config;
	config.memory.allocation = static_cast<Strata::Placement>(0xff);
	FlowResult result = flow.init(config, State::Idle);
	expect(
	    !result && result.status == FlowStatus::InvalidConfig,
	    "invalid allocation placement is rejected"
	);

	FlowConfig taskStackConfig;
	taskStackConfig.memory.taskStack = static_cast<Strata::Placement>(0xff);
	result = flow.init(taskStackConfig, State::Idle);
	expect(
	    !result && result.status == FlowStatus::InvalidConfig,
	    "invalid task stack placement is rejected"
	);
}

void testThreadSafeMutexUsesStrata() {
	Flow<State> flow;
	FlowConfig config;
	config.threadSafe = true;
	expect(static_cast<bool>(flow.init(config, State::Idle)), "thread-safe Flow initializes");
	expect(
	    flow.getDiagnostics().mutexControlRegion == Strata::Region::Unknown,
	    "generic host reports unknown mutex control region"
	);
	expect(static_cast<bool>(flow.deinit()), "thread-safe Flow deinitializes");
}

void testSetStateDoesNotAllocateThroughStrata() {
	Flow<State> flow;
	FlowConfig config;
	config.memory.allocation = Strata::Placement::Internal;
	config.maxStates = 2;
	config.maxTransitions = 1;
	expect(static_cast<bool>(flow.init(config, State::Idle)), "allocation-free runtime test initializes");
	expect(
	    flow.transition(State::Idle, State::Ready).action([]() {}).status() == FlowStatus::Ok,
	    "allocation-free runtime transition registers"
	);
	expect(
	    flow.onExit(State::Idle, []() {}) == FlowStatus::Ok,
	    "allocation-free runtime exit callback registers"
	);
	expect(
	    flow.onEnter(State::Ready, []() {}) == FlowStatus::Ok,
	    "allocation-free runtime enter callback registers"
	);

	Strata::resetAllocationDiagnostics();
	expect(flow.setState(State::Ready) == FlowStatus::Changed, "runtime state transition succeeds");
	const Strata::AllocationDiagnostics diagnostics = Strata::allocationDiagnostics();
	expect(diagnostics.attempts == 0, "setState performs no Strata allocation");
}
} // namespace

int main() {
	testDefaultMemoryPolicy();
	testPlacementCanChangeAcrossReinit();
	testUnsupportedRequiredExternalIsTransactional();
	testInvalidMemoryPolicyIsRejected();
	testThreadSafeMutexUsesStrata();
	testSetStateDoesNotAllocateThroughStrata();

	if (failureCount != 0) {
		std::printf("Flow memory tests failed: %d\n", failureCount);
		return 1;
	}
	std::printf("Flow memory tests passed\n");
	return 0;
}
