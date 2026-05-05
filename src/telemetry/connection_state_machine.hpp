#pragma once

#include <QObject>
#include <QString>
#include "core/types/common.hpp"

namespace aegis::telemetry {

/**
 * @brief Deterministic connection state machine for MAVLink UDP links.
 *
 * Owned and driven by MavlinkIO. Receives events (socket bound, packet received,
 * heartbeat received, heartbeat timeout, stop) and manages transitions between
 * six explicit states.
 *
 * Design:
 *  - Pure state logic — no socket ownership, no UI dependency.
 *  - Lives on the MAVLink I/O worker thread; signals auto-marshal via Qt.
 *  - Every transition is logged with a reason string for observability.
 *  - Testable without real UDP (event injection).
 */
class ConnectionStateMachine : public QObject {
    Q_OBJECT

public:
    enum class State {
        Disconnected,
        SocketBound,
        VehicleDiscovered,
        HeartbeatAlive,
        Degraded,
        Reconnecting
    };
    Q_ENUM(State)

    explicit ConnectionStateMachine(QObject* parent = nullptr);

    /** @brief Socket successfully bound. Transitions: Disconnected -> SocketBound. */
    void onSocketBound();

    /** @brief Any MAVLink packet received (not necessarily heartbeat). Transitions: SocketBound -> VehicleDiscovered. */
    void onPacketReceived();

    /** @brief Valid heartbeat received. Transitions any non-HeartbeatAlive state -> HeartbeatAlive. */
    void onHeartbeatReceived();

    /** @brief Heartbeat timer fired without a heartbeat.
     *  Transitions: HeartbeatAlive -> Degraded -> Reconnecting.
     */
    void onHeartbeatTimeout();

    /** @brief Link teardown. Transitions any state -> Disconnected. */
    void onStop();

    State currentState() const { return m_state; }

    /** @brief Human-readable name of a state. */
    static QString stateToString(State state);

    /** @brief Map internal state to public TelemetryBus ConnectionState. */
    static aegis::core::types::ConnectionState stateToConnectionState(State state);

signals:
    /** @brief Emitted whenever the state changes. */
    void stateChanged(aegis::telemetry::ConnectionStateMachine::State state);

    /**
     * @brief Emitted on every transition with old state, new state, and reason.
     *
     * Useful for audit logging and debugging.
     */
    void stateTransitioned(aegis::telemetry::ConnectionStateMachine::State oldState,
                           aegis::telemetry::ConnectionStateMachine::State newState,
                           const QString& reason);

private:
    void transitionTo(State newState, const QString& reason);

    State m_state{State::Disconnected};
};

} // namespace aegis::telemetry

Q_DECLARE_METATYPE(aegis::telemetry::ConnectionStateMachine::State)
