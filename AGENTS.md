# AEGIS GCS — Agent Engineering Rules

All agents, LLM workers, and human contributors must follow these rules. They are ranked by severity. A rule marked **CRITICAL** means violating it blocks merge. A rule marked **GUARDRAIL** means it should be checked during review.

---

## 1. Scope and File Discipline

**CRITICAL — Do not modify unrelated files.**
- Only touch files directly necessary for the assigned task.
- If you discover an unrelated bug, file it as a new task instead of fixing it in the same change.
- Do not refactor code in files you are not explicitly tasked to change.

**CRITICAL — Do not rewrite architecture unless the task explicitly asks for it.**
- Do not redesign the plugin system, telemetry bus, or state model as a side effect.
- If an architectural change seems necessary, escalate to the planner before implementing.

---

## 2. Plugin Architecture

**CRITICAL — Keep plugins runtime-loadable.**
- Plugins must remain shared libraries implementing `IPlugin`.
- The shell must discover and load them via `QPluginLoader`.
- Do not bake plugin logic into the main executable.
- Preserve Windows plugin deployment into `build/Release/plugins/`.

**GUARDRAIL — Plugin ABI changes require explicit task.**
- Changing `IPlugin` interface, `TelemetryBus` public API, or `VehicleState` public API must be its own task with full migration plan.

---

## 3. Telemetry and UI Decoupling

**CRITICAL — Keep telemetry and UI decoupled.**
- UI observes state; core services own state.
- Widgets must read from `VehicleState` or subscribe to `TelemetryBus` signals. They must never directly call MAVLink parsers or socket I/O.
- The telemetry layer (`MavlinkIO`, `MavlinkParser`) must not depend on Qt Widgets.

**CRITICAL — MAVLink parsing must remain separate from command execution.**
- `MavlinkParser` decodes inbound messages only.
- Outbound command construction and sending must live in a separate command service (`MavlinkCommander` or equivalent).
- Never mix parse logic with command logic in the same class.

---

## 4. Command Safety

**CRITICAL — All vehicle-control commands must go through a command service.**
- No widget or plugin may call `sendto()` on a socket directly.
- Commands must be wrapped in a typed `VehicleCommand` structure.
- The command service is responsible for sequencing, retry, and timeout.

**CRITICAL — Safety-critical commands require confirmation.**
- ARM, DISARM, mission upload, RTL, emergency stop, and any command that affects vehicle motion must require an explicit confirmation step.
- The confirmation mechanism must be enforced in the command service, not just the UI.

**GUARDRAIL — Command results must support ACK/failure reporting.**
- Every command must produce a result: pending, acknowledged, rejected, timeout, or failed.
- Results must be emitted on `TelemetryBus` so any plugin can observe them.

---

## 5. State and Connection

**CRITICAL — UI observes state; core services own state.**
- `VehicleState` is the single source of truth for vehicle data.
- `TelemetryBus` is the single source of truth for events.
- Widgets read from these sources; they do not maintain shadow state that contradicts them.

**GUARDRAIL — State transitions must be deterministic.**
- Connection state machine must have explicit states, explicit transitions, and no hidden implicit jumps.

---

## 6. Testing

**CRITICAL — Add or update tests for behavior changes.**
- Any change that modifies observable behavior must include a test.
- Tests may be unit tests (GoogleTest), Qt tests, or replay-based integration tests.
- If a change is not easily unit-testable, add a replay test or explain why in the run log.

**GUARDRAIL — Prefer testing without real hardware/UDP where possible.**
- Use simulated heartbeat injection, replay files, or mock sockets.
- `MavlinkIO` and connection state machine must be testable without a live UDP endpoint.

---

## 7. Change Size and Review

**CRITICAL — Keep each change PR-sized.**
- One task = one focused change.
- If a task seems to require touching more than ~8 files, split it into smaller tasks.
- Maximum reasonable diff: ~400 lines changed per task (including tests).

**GUARDRAIL — Preserve Windows plugin deployment into `Release/plugins`.**
- Do not change CMake output directories or plugin copy logic unless the task explicitly covers deployment.
- The working `build.bat` must continue to produce runnable binaries.

---

## 8. Technology Stack

**CRITICAL — Project must remain Qt6/C++20 compatible.**
- Do not use Qt5-only APIs.
- Do not use C++23 or later features.
- Prefer `std::` over `boost::` where standard library equivalents exist.
- Prefer Qt containers and types in Qt-facing code; prefer `std::` in pure logic code.

**GUARDRAIL — Minimize new dependencies.**
- Any new third-party library requires a separate task with justification, license check, and build integration plan.

---

## 9. Logging and Observability

**GUARDRAIL — Use the existing logger; do not invent new logging systems.**
- The custom thread-safe logger is the unified logging facility.
- Structured logs must use the existing log levels: Debug, Info, Warning, Error, Critical.

---

## 10. Safety-Critical Code Style

**GUARDRAIL — Explicit over implicit.**
- No hidden default actions for safety-critical paths.
- No silent failures for commands.
- Timeouts must be explicit and configurable.
- State machines must log every transition at Info or higher level.

---

## Violation Escalation

If an agent cannot follow a CRITICAL rule without violating the task acceptance criteria, it must:
1. Stop implementation.
2. Log the conflict in `orchestration/runs/`.
3. Return control to the orchestrator for replanning.

Do not "work around" CRITICAL rules.
