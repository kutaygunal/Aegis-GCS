#pragma once

#include <QObject>
#include <QReadWriteLock>
#include <QHash>
#include <QMultiHash>
#include <QMetaType>
#include <functional>
#include <memory>
#include "../types/common.hpp"

namespace aegis::core {

/**
 * @brief Central nervous system of AEGIS.
 *
 * All telemetry, state deltas, alerts, and commands flow through this bus.
 * Uses Qt Signals for automatic cross-thread marshaling via QueuedConnection.
 *
 * Design decisions:
 *  - Qt Signals prevent UI deadlocks when telemetry arrives on the MAVLink I/O thread.
 *  - Topic strings keep the bus interface stable as new vehicle types are added.
 *  - Rate-limited throttling can be applied per-consumer to prevent signal saturation.
 */
class TelemetryBus : public QObject {
    Q_OBJECT

public:
    explicit TelemetryBus(QObject* parent = nullptr);
    ~TelemetryBus() override;

    // ── Typed vehicle telemetry (hot path) ─────────────────────────────
    void emitAttitudeChanged(const types::AttitudeData& data);
    void emitPositionChanged(const types::PositionData& data);
    void emitHeartbeat(const types::SystemState& state);
    void emitStatusText(const QString& text);
    void emitBatteryChanged(const types::BatteryData& data);
    void emitMissionCurrentChanged(int index);

    // ── System events ───────────────────────────────────────────────
    void emitAlert(types::AlertLevel level, const QString& message);
    void emitConnectionStateChanged(types::ConnectionState state);
    void emitCommandResponse(quint16 commandId, bool success, const QString& message);
    void emitDiagnosticExportRequested(const QString& path);

    // ── Generic topic-based pub/sub ───────────────────────────────────
    void subscribe(const QString& topic, QObject* subscriber,
                   std::function<void(const QVariant&)> callback);
    void unsubscribe(const QString& topic, QObject* subscriber);
    void publish(const QString& topic, const QVariant& data);

    // ── Command outbound ──────────────────────────────────────────────
    void postCommand(const aegis::core::types::VehicleCommand& cmd);

signals:
    void attitudeChanged(const aegis::core::types::AttitudeData& data);
    void positionChanged(const aegis::core::types::PositionData& data);
    void heartbeatReceived(const aegis::core::types::SystemState& state);
    void statusTextReceived(const QString& text);
    void batteryChanged(const aegis::core::types::BatteryData& data);
    void missionCurrentChanged(int index);

    void alertRaised(aegis::core::types::AlertLevel level, const QString& message);
    void connectionStateChanged(aegis::core::types::ConnectionState state);
    void commandResponse(quint16 commandId, bool success, const QString& message);
    void diagnosticExportRequested(const QString& path);

    void outboundCommand(const aegis::core::types::VehicleCommand& cmd);

    /** Internal bridge for generic topic dispatch. */
    void topicEmitted(const QString& topic, const QVariant& payload);

private slots:
    void dispatchTopic(const QString& topic, const QVariant& payload);

private:
    QReadWriteLock m_lock;
    // topic -> list of {subscriber QObject*, callback}
    QMultiHash<QString, std::pair<QObject*, std::function<void(const QVariant&)>>> m_subs;
};

} // namespace aegis::core
