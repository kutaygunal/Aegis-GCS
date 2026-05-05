# Reviewer Output — P1-001: Formal Connection State Machine

## A. Acceptance Criteria

- [x] Connection states include socket_bound, vehicle_discovered, heartbeat_alive, degraded, disconnected, reconnecting.
  - Verified: `ConnectionStateMachine::State` defines all 6. `ConnectionState` enum in `common.hpp` maps all 6 plus `Error`.
- [x] State transitions are deterministic and unit-tested.
  - Verified: `test_connection_state_machine.cpp` covers 12 scenarios including normal flow, timeouts, recovery, stop, and edge cases.
- [x] Timeout behavior is covered by simulated heartbeat or replay tests.
  - Verified: `TimeoutToDegraded`, `TimeoutToReconnecting`, `RecoverFromDegraded`, `RecoverFromReconnecting`, `DoubleTimeoutFromHeartbeatAlive` all pass.
- [x] UI receives state updates through the telemetry bus.
  - Verified: `MavlinkIO` → `Application` lambda → `TelemetryBus::emitConnectionStateChanged` → `MainWindow::updateConnectionState` → `ConnectionBar::setConnectionState`.
- [x] State machine can be tested without real UDP.
  - Verified: `test_connection_state_machine` is pure unit test with no UDP socket.

## B. Architecture Compliance (AGENTS.md)

- [x] No unrelated files modified.
  - All 14 touched files are directly related to connection state formalization.
- [x] Architecture was not rewritten as a side effect.
  - The I/O thread model, plugin system, and parser remain unchanged.
- [x] Plugins remain runtime-loadable.
  - `IPlugin`, `QPluginLoader`, and `TelemetryBus` public API are preserved.
- [x] Telemetry and UI remain decoupled.
  - UI reads from `TelemetryBus` only; no widget touches socket I/O.
- [x] MAVLink parsing is separate from connection-state transitions.
  - `MavlinkParser` is untouched. State machine receives "heartbeat received" event, not raw MAVLink bytes.
- [x] State is owned by core services; UI observes only.
  - `ConnectionStateMachine` lives in telemetry layer. UI observes via bus.
- [x] All vehicle-control commands go through a command service.
  - Not applicable to this task; no command paths were modified.
- [x] Safety-critical commands have confirmation.
  - Not applicable to this task.
- [x] Command results support ACK/failure reporting.
  - Not applicable to this task.

## C. Safety and Correctness

- [x] No silent failures on safety-critical paths.
  - State transitions are logged at `qDebug` level with explicit reasons.
- [x] Timeouts are explicit and bounded.
  - `m_heartbeatTimeoutMs` is used; no unbounded waits introduced.
- [x] No use-after-free, race conditions, or lifetime issues.
  - `ConnectionStateMachine` is parented to `MavlinkIO` (`this`). Deleted with `deleteLater()` or via parent chain in `stop()`.
- [x] No hidden implicit state transitions.
  - Every transition goes through `transitionTo()` which logs old state, new state, and reason.
- [x] Thread ownership is clear.
  - `ConnectionStateMachine` created on worker thread in `setupSocket()`. Signals cross to main thread via `QueuedConnection` through `Application` wiring.

## D. Testing

- [x] Tests were added or updated for behavior changes.
  - 12 new unit tests + 3 updated integration tests.
- [x] Tests compile and pass.
  - `test_connection_state_machine`: 12/12 PASS
  - `test_mavlink_io`: 3/3 PASS
- [x] Edge cases are covered.
  - Spurious heartbeat while disconnected, double timeout, stop from any state, no duplicate transitions.
- [x] Tests can run without real hardware/UDP where possible.
  - State machine tests are pure unit tests. MavlinkIO tests use loopback UDP.

## E. Maintainability

- [x] Change is PR-sized.
  - 14 files, ~240 net lines. Focused on a single concern.
- [x] Code style matches surrounding files.
  - Follows existing Qt naming, indentation, and header style.
- [x] New code has adequate inline comments.
  - State machine header documents every public method. Transition reasons are descriptive strings.
- [x] No unnecessary new dependencies.
  - No new third-party libraries.
- [x] Qt6/C++20 compatibility maintained.
  - Uses only Qt6 APIs and C++20 features already present in the codebase.

## F. Windows Plugin Deployment

- [x] CMake plugin copy logic is preserved.
  - No changes to plugin CMakeLists.txt or deployment rules.
- [x] build.bat still works.
  - Main executable `aegis.exe` builds successfully in Release.
- [x] Plugins still deploy to `build/Release/plugins/`.
  - Verified: plugin DLLs are present in `build/Release/plugins/` after build.

---

## DECISION: APPROVE

**SUMMARY:** P1-001 implements a clean, deterministic 6-state connection state machine that replaces the previous implicit boolean logic. The change is well-tested (15 passing tests), preserves architecture boundaries, and propagates typed state changes through the existing TelemetryBus. No safety issues, no architecture violations, no unrelated changes.

**STRENGTHS:**
  - State machine is pure logic, testable without UDP, and clearly separated from I/O and UI.
  - Transition logging with reason strings provides excellent observability.
  - The `stateToConnectionState()` mapping cleanly bridges internal state to public bus API.
  - Existing UDP integration test was meaningfully expanded to cover SocketBound and VehicleDiscovered states.
  - ConnectionBar color coding immediately improves operator awareness of link health.

**NOTES:**
  - The `Reconnecting` state currently stays in reconnecting on additional timeouts. A future task should add a max-retry counter to transition to `Disconnected`.
  - Pre-existing tests (`test_telemetry_bus`, `test_ring_buffer`) have compilation issues unrelated to this task. Recommend a separate infrastructure cleanup task.
  - Consider adding a config key for `degradedTimeoutMultiplier` (e.g., 1x heartbeat timeout for degraded, 2x for reconnecting) in a future task.
