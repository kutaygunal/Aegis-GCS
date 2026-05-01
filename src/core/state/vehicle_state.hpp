#pragma once

#include <QObject>
#include <QReadWriteLock>
#include <QVariant>
#include "../types/common.hpp"

namespace aegis::core {

/**
 * @brief Canonical, thread-safe ground truth for the active vehicle.
 *
 * All telemetry consumers read from this model.
 * Mutations are serialized through the TelemetryBus; direct mutation outside
 * the bus thread is allowed only via update*() methods which lock internally.
 */
class VehicleState : public QObject {
    Q_OBJECT

public:
    explicit VehicleState(QObject* parent = nullptr);
    ~VehicleState() override;

    // ── Properties (thread-safe getters) ────────────────────────────────
    types::AttitudeData attitude() const;
    types::PositionData position() const;
    types::BatteryData battery() const;
    types::SystemState systemState() const;
    int missionCurrent() const;

    // ── Mutators (call from TelemetryBus handlers) ────────────────────
    void updateAttitude(const types::AttitudeData& data);
    void updatePosition(const types::PositionData& data);
    void updateBattery(const types::BatteryData& data);
    void updateSystemState(const types::SystemState& state);
    void updateMissionCurrent(int index);

signals:
    void attitudeChanged(const aegis::core::types::AttitudeData& data);
    void positionChanged(const aegis::core::types::PositionData& data);
    void batteryChanged(const aegis::core::types::BatteryData& data);
    void systemStateChanged(const aegis::core::types::SystemState& state);
    void missionCurrentChanged(int index);
    void stateDomainChanged(const QString& domain, const QVariant& value);

private:
    mutable QReadWriteLock m_lock;
    types::AttitudeData m_attitude;
    types::PositionData m_position;
    types::BatteryData m_battery;
    types::SystemState m_systemState;
    int m_missionCurrent{0};
};

} // namespace aegis::core
