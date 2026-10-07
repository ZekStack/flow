# Diagnostics

`getDiagnostics()` returns counters and last request fields.

```cpp
FlowDiag<State> diag = flow.getDiagnostics();
Serial.println(diag.changedCount);
Serial.println(flow.statusToString(diag.lastStatus));
```

Diagnostics include current and previous state, transition count, request counters, failure counters, last status, last requested state, last from/to states, and last change time from `millis()`.

Memory diagnostics also expose `allocationPlacement`, `stateStorageRegion`, `transitionStorageRegion`, and `mutexControlRegion`. Placement records caller intent; region records where Strata actually placed the storage. On generic host builds observed regions are `Unknown`. When thread safety is disabled, `mutexControlRegion` is also `Unknown`.

Before initialization, diagnostics return default values with `lastStatus = FlowStatus::NotInitialized`.
