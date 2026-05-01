#include "vehicle_state.hpp"

namespace aegis::core {

VehicleState::VehicleState(QObject* parent) : QObject(parent) {}
VehicleState::~VehicleState() = default;

// ── Getters ─────────────────────────────────────────────────────────
types::AttitudeData VehicleState::attitude() const {
    QReadLocker lock(&m_lock);
    return m_attitude;
}

types::PositionData VehicleState::position() const {
    QReadLocker lock(&m_lock);
    return m_position;
}

types::BatteryData VehicleState::battery() const {
    QReadLocker lock(&m_lock);
    return m_battery;
}

types::SystemState VehicleState::systemState() const {
    QReadLocker lock(&m_lock);
    return m_systemState;
}

int VehicleState::missionCurrent() const {
    QReadLocker lock(&m_lock);
    return m_missionCurrent;
}

// ── Mutators ────────────────────────────────────────────────────────
void VehicleState::updateAttitude(const types::AttitudeData& data) {
    {
        QWriteLocker lock(&m_lock);
        m_attitude = data;
    }
    emit attitudeChanged(data);
    emit stateDomainChanged("attitude", QVariant::fromValue(data));
}

void VehicleState::updatePosition(const types::PositionData& data) {
    {
        QWriteLocker lock(&m_lock);
        m_position = data;
    }
    emit positionChanged(data);
    emit stateDomainChanged("position", QVariant::fromValue(data));
}

void VehicleState::updateBattery(const types::BatteryData& data) {
    {
        QWriteLocker lock(&m_lock);
        m_battery = data;
    }
    emit batteryChanged(data);
    emit stateDomainChanged("battery", QVariant::fromValue(data));
}

void VehicleState::updateSystemState(const types::SystemState& state) {
    {
        QWriteLocker lock(&m_lock);
        m_systemState = state;
    }
    emit systemStateChanged(state);
    emit stateDomainChanged("system", QVariant::fromValue(state));
}

void VehicleState::updateMissionCurrent(int index) {
    {
        QWriteLocker lock(&m_lock);
        m_missionCurrent = index;
    }
    emit missionCurrentChanged(index);
    emit stateDomainChanged("mission_current", index);
}

} // namespace aegis::core
