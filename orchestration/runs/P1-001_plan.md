# Planner Output — P1-001: Formal Connection State Machine

## 1. Summary

This task replaces the implicit boolean connection logic (`m_linkAlive`) in `MavlinkIO` with an explicit `ConnectionStateMachine` class supporting 6 deterministic states. The state machine receives events from the I/O layer, manages transitions, and emits typed state changes via the `TelemetryBus`. This enables future safety features (failsafe alerting, command gating) to react to connection health with granularity beyond "connected/disconnected."

## 2. Scope Boundary

**In scope:**
- Create `ConnectionStateMachine` class (6 states, event-driven transitions)
- Expand `ConnectionState` enum in `common.hpp` from 4 to 6+ values
- Replace `m_linkAlive` bool in `MavlinkIO` with state machine events
- Update `Application` wiring to forward state machine signals to `TelemetryBus`
- Update `ConnectionBar` to display all states with color coding
- Add unit tests for all state transitions (normal, timeout, reconnect, degraded, disconnect)
- Update existing `test_mavlink_io.cpp` to use new state enum

**Out of scope:**
- Command service / ACK handling (P2-001)
- Failsafe monitor (P2-003)
- Reconnection retry policy with backoff (the `Reconnecting` state exists, but the retry loop is future work)
- New config keys for state machine thresholds
- Changing the socket I/O thread model

**Architecture touch:** YES — `ConnectionState` enum is a public type used by `TelemetryBus`. This is a controlled expansion (adding enum values), not a breaking change. Old consumers that only handled `Disconnected`/`Connected` will still compile; they will simply see new states they may not handle explicitly.

## 3. Affected Files

| File | Change | Why |
|---|---|---|
| `src/core/types/common.hpp` | Modify | Expand `ConnectionState` enum |
| `src/telemetry/connection_state_machine.hpp` | Create | New state machine class |
| `src/telemetry/connection_state_machine.cpp` | Create | State machine implementation |
| `src/telemetry/mavlink_io.hpp` | Modify | Replace bool with state machine pointer |
| `src/telemetry/mavlink_io.cpp` | Modify | Feed events to state machine |
| `src/telemetry/CMakeLists.txt` | Modify | Add new source files |
| `src/app/application.cpp` | Modify | Wire state machine to TelemetryBus |
| `src/ui/widgets/connection_bar.hpp` | Modify | Accept full `ConnectionState` enum |
| `src/ui/widgets/connection_bar.cpp` | Modify | Display all states with colors |
| `src/ui/main_window.hpp` | Modify | Update slot to pass `ConnectionState` through |
| `src/ui/main_window.cpp` | Modify | Forward `ConnectionState` to `ConnectionBar` |
| `tests/telemetry/test_connection_state_machine.cpp` | Create | Unit tests |
| `tests/telemetry/test_mavlink_io.cpp` | Modify | Update for new state enum |
| `tests/CMakeLists.txt` | Modify | Add new test executable |

## 4. New Files

### `src/telemetry/connection_state_machine.hpp`
- **Responsibility:** Own connection state transitions; testable without UDP.
- **Key interface:**
  - `enum class State { Disconnected, SocketBound, VehicleDiscovered, HeartbeatAlive, Degraded, Reconnecting }`
  - `void onSocketBound()`, `void onPacketReceived()`, `void onHeartbeatReceived()`, `void onHeartbeatTimeout()`, `void onStop()`
  - `State currentState() const`
  - Signals: `stateChanged(State)`, `stateTransitioned(State old, State new, QString reason)`

### `src/telemetry/connection_state_machine.cpp`
- **Responsibility:** Implements deterministic transition logic per state+event table.

### `tests/telemetry/test_connection_state_machine.cpp`
- **Responsibility:** Unit tests for all transitions, timeout sequences, and edge cases.

## 5. Interface Design

**New `ConnectionStateMachine` public API:**
```cpp
enum class State { Disconnected, SocketBound, VehicleDiscovered, HeartbeatAlive, Degraded, Reconnecting };

void onSocketBound();        // Disconnected -> SocketBound
void onPacketReceived();     // SocketBound -> VehicleDiscovered
void onHeartbeatReceived();  // Any non-HeartbeatAlive -> HeartbeatAlive
void onHeartbeatTimeout();   // HeartbeatAlive -> Degraded -> Reconnecting
void onStop();               // Any -> Disconnected
```

**Updated `ConnectionState` enum:**
```cpp
enum class ConnectionState {
    Disconnected,
    SocketBound,
    VehicleDiscovered,
    HeartbeatAlive,
    Degraded,
    Reconnecting,
    Error
};
```

**Updated `MavlinkIO` signal:**
```cpp
// OLD: void connectionStateChanged(bool connected);
// NEW:
void connectionStateChanged(aegis::core::types::ConnectionState state);
```

**Updated `ConnectionBar` API:**
```cpp
// OLD: void setConnected(bool connected);
// NEW:
void setConnectionState(aegis::core::types::ConnectionState state);
```

## 6. State / Data Flow

```
[MavlinkIO::start()]
  -> onSocketBound() -> [StateMachine: Disconnected -> SocketBound]
    -> emit stateChanged(SocketBound)

[MavlinkIO::onReadyRead() first packet]
  -> onPacketReceived() -> [StateMachine: SocketBound -> VehicleDiscovered]
    -> emit stateChanged(VehicleDiscovered)

[MavlinkIO::onReadyRead() heartbeat]
  -> onHeartbeatReceived() -> [StateMachine: * -> HeartbeatAlive]
    -> emit stateChanged(HeartbeatAlive)

[MavlinkIO::onHeartbeatTimeout()]
  -> onHeartbeatTimeout() -> [StateMachine: HeartbeatAlive -> Degraded]
    -> emit stateChanged(Degraded)
  -> (second timeout) -> [StateMachine: Degraded -> Reconnecting]
    -> emit stateChanged(Reconnecting)

[MavlinkIO::stop()]
  -> onStop() -> [StateMachine: * -> Disconnected]
    -> emit stateChanged(Disconnected)
```

**Application wiring:**
```
MavlinkIO::connectionStateChanged(ConnectionState)
  -> Application lambda
    -> TelemetryBus::emitConnectionStateChanged(ConnectionState)
      -> MainWindow::updateConnectionState(ConnectionState)
        -> ConnectionBar::setConnectionState(ConnectionState)
```

## 7. Risk Analysis

| Risk | Likelihood | Mitigation | Fallback |
|---|---|---|---|
| Signal signature change breaks external consumers | Low | `ConnectionState` enum expansion is backward-compatible at compile time (old switch cases still valid) | Add deprecated `bool` overload if needed |
| UI plugins rely on `ConnectionState::Connected` only | Medium | Update `ConnectionBar` to handle all states; other plugins receive the enum and can choose to handle new states or not | Default behavior: any non-Disconnected state shows as "active" |
| State machine logic error causes flapping | Medium | Unit tests cover all transitions; hysteresis via Degraded state | Reviewer should verify transition table completeness |
| Threading issues (state machine on worker thread vs bus on main thread) | Low | `ConnectionStateMachine` lives on the same worker thread as `MavlinkIO`; state changes are emitted as Qt signals which auto-marshal via `QueuedConnection` | Ensure Application connects with `Qt::QueuedConnection` |

## 8. Test Strategy

**Unit tests (`test_connection_state_machine`):**
- `NormalSequence`: Disconnected -> SocketBound -> VehicleDiscovered -> HeartbeatAlive
- `TimeoutToDegraded`: HeartbeatAlive -> Degraded (one timeout)
- `TimeoutToReconnecting`: Degraded -> Reconnecting (second timeout)
- `RecoverFromDegraded`: Degraded -> HeartbeatAlive (heartbeat arrives)
- `RecoverFromReconnecting`: Reconnecting -> HeartbeatAlive (heartbeat arrives)
- `StopFromAnyState`: stop() from each state -> Disconnected
- `HeartbeatWhileDisconnected`: no transition (heartbeat before socket bound)
- `DoubleTimeoutFromHeartbeatAlive`: HeartbeatAlive -> Degraded -> Reconnecting

**Integration tests (`test_mavlink_io` update):**
- Existing UDP heartbeat test updated to expect `ConnectionState::HeartbeatAlive` instead of `bool true`
- Timeout test updated to expect `ConnectionState::Reconnecting` (after degraded grace)

**Approximate test count:** 10 new unit tests + 2 updated integration tests.

## 9. Open Questions

None. The current heartbeat timeout behavior is well-understood and the state machine formalizes it without changing the underlying I/O thread model.

## 10. Dependencies Check

All `depends_on` tasks are complete (empty list). P1-001 is the first task in Sprint 1 and has no dependencies. Proceeding with implementation.
