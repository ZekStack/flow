# Diagnostics

`getDiagnostics()` returns counters and last request fields.

```cpp
FlowDiag<State> diag = flow.getDiagnostics();
Serial.println(diag.changedCount);
Serial.println(flow.statusToString(diag.lastStatus));
```

Diagnostics include current and previous state, transition count, request counters, failure counters, last status, last requested state, last from/to states, and last change time from `millis()`.

Before initialization, diagnostics return default values with `lastStatus = FlowStatus::NotInitialized`.
