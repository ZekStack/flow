# Guards

Guards decide whether a transition may happen.

```cpp
flow.transition(State::Connecting, State::Connected)
    .guard([]() {
	    return WiFi.isConnected();
    });
```

When a guard returns `false`, `setState()` returns `FlowStatus::GuardRejected` and the current state does not change.

Guards should be short and should not start long-running work. Start work in transition actions or enter callbacks instead.
