#include "telemetry_bus.hpp"
#include <QDebug>

namespace aegis::core {

TelemetryBus::TelemetryBus(QObject* parent) : QObject(parent) {
    qRegisterMetaType<types::AttitudeData>(">aegis::core::types::AttitudeData");
    qRegisterMetaType<types::PositionData>("aegis::core::types::PositionData");
    qRegisterMetaType<types::BatteryData>("aegis::core::types::BatteryData");
    qRegisterMetaType<types::SystemState>("aegis::core::types::SystemState");
    qRegisterMetaType<types::VehicleCommand>("aegis::core::types::VehicleCommand");
    qRegisterMetaType<types::AlertLevel>("aegis::core::types::AlertLevel");
    qRegisterMetaType<types::ConnectionState>("aegis::core::types::ConnectionState");

    connect(this, &TelemetryBus::topicEmitted,
            this, &TelemetryBus::dispatchTopic, Qt::QueuedConnection);
}

TelemetryBus::~TelemetryBus() = default;

// ── Vehicle telemetry emitters ──────────────────────────────────────────
void TelemetryBus::emitAttitudeChanged(const types::AttitudeData& data) {
    emit attitudeChanged(data);
}

void TelemetryBus::emitPositionChanged(const types::PositionData& data) {
    emit positionChanged(data);
}

void TelemetryBus::emitHeartbeat(const types::SystemState& state) {
    emit heartbeatReceived(state);
}

void TelemetryBus::emitStatusText(const QString& text) {
    emit statusTextReceived(text);
}

void TelemetryBus::emitBatteryChanged(const types::BatteryData& data) {
    emit batteryChanged(data);
}

void TelemetryBus::emitMissionCurrentChanged(int index) {
    emit missionCurrentChanged(index);
}

// ── System events ─────────────────────────────────────────────────────
void TelemetryBus::emitAlert(types::AlertLevel level, const QString& message) {
    emit alertRaised(level, message);
}

void TelemetryBus::emitConnectionStateChanged(types::ConnectionState state) {
    emit connectionStateChanged(state);
}

void TelemetryBus::emitCommandResponse(quint16 commandId, bool success,
                                       const QString& message) {
    emit commandResponse(commandId, success, message);
}

// ── Generic topic pub/sub ─────────────────────────────────────────────
void TelemetryBus::subscribe(const QString& topic, QObject* subscriber,
                             std::function<void(const QVariant&)> callback) {
    QWriteLocker lock(&m_lock);
    m_subs.insert(topic, {subscriber, std::move(callback)});
}

void TelemetryBus::unsubscribe(const QString& topic, QObject* subscriber) {
    QWriteLocker lock(&m_lock);
    auto it = m_subs.find(topic);
    while (it != m_subs.end() && it.key() == topic) {
        if (it.value().first == subscriber) {
            it = m_subs.erase(it);
        } else {
            ++it;
        }
    }
}

void TelemetryBus::publish(const QString& topic, const QVariant& data) {
    emit topicEmitted(topic, data);
}

// ── Command outbound ────────────────────────────────────────────────────
void TelemetryBus::postCommand(const types::VehicleCommand& cmd) {
    emit outboundCommand(cmd);
}

// ── Private slots ─────────────────────────────────────────────────────
void TelemetryBus::dispatchTopic(const QString& topic, const QVariant& payload) {
    QReadLocker lock(&m_lock);
    auto range = m_subs.equal_range(topic);
    for (auto it = range.first; it != range.second; ++it) {
        if (auto* obj = it.value().first; obj && !obj->isNull()) {
            try {
                it.value().second(payload);
            } catch (...) {
                // TODO: log to centralized crash handler instead of swallowing
            }
        }
    }
}

} // namespace aegis::core
