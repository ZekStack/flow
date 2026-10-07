#!/usr/bin/env python3

from pathlib import Path
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
errors: list[str] = []


def require(path: str, text: str, message: str) -> None:
    content = (ROOT / path).read_text(encoding="utf-8")
    if text not in content:
        errors.append(message)


def reject(pattern: str, message: str) -> None:
    regex = re.compile(pattern)
    for path in SRC.rglob("*"):
        if not path.is_file() or path.suffix not in {".h", ".hpp", ".cpp", ".inc"}:
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
            if regex.search(line):
                errors.append(
                    f"{path.relative_to(ROOT)}:{number}: {message}: {line.strip()}"
                )


metadata = json.loads((ROOT / "library.json").read_text(encoding="utf-8"))
expected = "https://github.com/ZekStack/strata.git#v0.1.4"
if metadata.get("dependencies", {}).get("Strata") != expected:
    errors.append("library.json must pin Strata v0.1.4")

require(
    "src/Flow.h",
    "Strata::MemoryPolicy memory",
    "FlowConfig must expose Strata::MemoryPolicy",
)
require(
    "src/Flow.h",
    "FlowStorage<StateEntry>",
    "Flow state storage must use the Strata-backed owner",
)
require(
    "src/Flow.h",
    "FlowStorage<TransitionEntry>",
    "Flow transition storage must use the Strata-backed owner",
)
require(
    "src/internal/FlowStorage.h",
    "Strata::allocateArray<T>",
    "Flow typed storage must allocate through Strata",
)
require(
    "src/internal/FlowMutex.h",
    "Strata::FreeRTOS::RecursiveMutex",
    "Flow mutex ownership must use Strata FreeRTOS ownership",
)

reject(r"\bheap_caps_", "direct ESP-IDF heap allocation is forbidden")
reject(r"\bMALLOC_CAP_", "direct ESP-IDF heap capability use is forbidden")
reject(r"\bps_malloc\b", "direct PSRAM allocation is forbidden")
reject(
    r"\bxSemaphoreCreate(?:Mutex|RecursiveMutex)\s*\(",
    "owned mutex creation must use Strata",
)
reject(r"(?<![:\w])new\s*\(\s*std::nothrow", "direct nothrow new ownership is forbidden")
reject(r"(?<![:\w])new\s+(?!\()", "direct new ownership is forbidden")
reject(r"(?<![:\w])delete\s*\[", "direct array delete ownership is forbidden")
reject(r"\b(?:malloc|calloc|realloc|free)\s*\(", "direct C heap ownership is forbidden")

if errors:
    print("\n".join(errors))
    sys.exit(1)

print("Flow Strata ownership contract passed")
