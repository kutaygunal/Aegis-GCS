#include "connection_state_machine.hpp"
#include <QDebug>

namespace aegis::telemetry {

ConnectionStateMachine::ConnectionStateMachine(QObject* parent)
    : QObject(parent) {
    qRegisterMetaType<State>("aegis::telemetry::ConnectionStateMachine::State");
}

void ConnectionStateMachine::onSocketBound() {
    if (m_state == State::Disconnected) {
        transitionTo(State::SocketBound, QStringLiteral("UDP socket bound"));
    }
}

void ConnectionStateMachine::onPacketReceived() {
    if (m_state == State::SocketBound) {
        transitionTo(State::VehicleDiscovered,
                     QStringLiteral("First MAVLink packet received; remote auto-detected"));
    }
}

void ConnectionStateMachine::onHeartbeatReceived() {
    switch (m_state) {
        case State::SocketBound:
            transitionTo(State::VehicleDiscovered,
                         QStringLiteral("Heartbeat received before packet detection"));
            // Fall through intentionally: heartbeat also promotes to HeartbeatAlive
            transitionTo(State::HeartbeatAlive,
                         QStringLiteral("Heartbeat valid; link alive"));
            break;
        case State::VehicleDiscovered:
            transitionTo(State::HeartbeatAlive,
                         QStringLiteral("Heartbeat valid; link alive"));
            break;
        case State::Degraded:
            transitionTo(State::HeartbeatAlive,
                         QStringLiteral("Heartbeat recovered from degraded state"));
            break;
        case State::Reconnecting:
            transitionTo(State::HeartbeatAlive,
                         QStringLiteral("Heartbeat recovered from reconnecting state"));
            break;
        case State::HeartbeatAlive:
            // Stay in HeartbeatAlive; heartbeat is still valid
            break;
        case State::Disconnected:
            // Ignore: heartbeat received while disconnected (spurious)
            break;
    }
}

void ConnectionStateMachine::onHeartbeatTimeout() {
    switch (m_state) {
        case State::HeartbeatAlive:
            transitionTo(State::Degraded,
                         QStringLiteral("Heartbeat timeout; entering degraded mode"));
            break;
        case State::Degraded:
            transitionTo(State::Reconnecting,
                         QStringLiteral("Grace period exceeded; entering reconnecting mode"));
            break;
        case State::Reconnecting:
            // Stay in Reconnecting on subsequent timeouts
            // Future: count retries and transition to Disconnected after max attempts
            break;
        case State::SocketBound:
        case State::VehicleDiscovered:
            transitionTo(State::Reconnecting,
                         QStringLiteral("Heartbeat timeout before first heartbeat; reconnecting"));
            break;
        case State::Disconnected:
            // Ignore
            break;
    }
}

void ConnectionStateMachine::onStop() {
    if (m_state != State::Disconnected) {
        transitionTo(State::Disconnected, QStringLiteral("Link stopped"));
    }
}

void ConnectionStateMachine::transitionTo(State newState, const QString& reason) {
    if (m_state == newState) {
        return;
    }

    const State oldState = m_state;
    m_state = newState;

    qDebug() << "[ConnectionStateMachine]" << stateToString(oldState) << "->"
             << stateToString(newState) << "|" << reason;

    emit stateChanged(newState);
    emit stateTransitioned(oldState, newState, reason);
}

QString ConnectionStateMachine::stateToString(State state) {
    switch (state) {
        case State::Disconnected:      return QStringLiteral("Disconnected");
        case State::SocketBound:       return QStringLiteral("SocketBound");
        case State::VehicleDiscovered: return QStringLiteral("VehicleDiscovered");
        case State::HeartbeatAlive:    return QStringLiteral("HeartbeatAlive");
        case State::Degraded:          return QStringLiteral("Degraded");
        case State::Reconnecting:      return QStringLiteral("Reconnecting");
    }
    return QStringLiteral("Unknown");
}

aegis::core::types::ConnectionState ConnectionStateMachine::stateToConnectionState(State state) {
    using BusState = aegis::core::types::ConnectionState;
    switch (state) {
        case State::Disconnected:      return BusState::Disconnected;
        case State::SocketBound:       return BusState::SocketBound;
        case State::VehicleDiscovered: return BusState::VehicleDiscovered;
        case State::HeartbeatAlive:    return BusState::HeartbeatAlive;
        case State::Degraded:          return BusState::Degraded;
        case State::Reconnecting:      return BusState::Reconnecting;
    }
    return BusState::Error;
}

} // namespace aegis::telemetry
