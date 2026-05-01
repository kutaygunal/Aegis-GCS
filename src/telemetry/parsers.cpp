#include "parsers.hpp"
#include "core/bus/telemetry_bus.hpp"
#include "core/state/vehicle_state.hpp"
#include <QDebug>

// In a real build, include MAVLink C headers:
// #include <mavlink.h>

namespace aegis::telemetry {

MavlinkParser::MavlinkParser(TelemetryBus* bus, VehicleState* state,
                             QObject* parent)
    : QObject(parent), m_bus(bus), m_state(state) {}

void MavlinkParser::processMessage(const types::MavlinkMessage& msg) {
    // Placeholder dispatch based on msgid.
    // With MAVLink C library, use mavlink_decode_message() and switch(msg.msgid).
    switch (msg.msgid) {
        case 0:   parseHeartbeat(msg); break;
        case 30:  parseAttitude(msg); break;
        case 33:  parseGlobalPosition(msg); break;
        case 147: parseBatteryStatus(msg); break;
        case 42:  parseMissionCurrent(msg); break;
        case 253: parseStatusText(msg); break;
        default: break;
    }
}

void MavlinkParser::parseHeartbeat(const types::MavlinkMessage& msg) {
    core::types::SystemState state;
    // TODO: decode MAVLink HEARTBEAT into state fields
    Q_UNUSED(msg)
    state.systemStatus = core::types::SystemStatus::Active;  // placeholder
    m_state->updateSystemState(state);
    m_bus->emitHeartbeat(state);
}

void MavlinkParser::parseAttitude(const types::MavlinkMessage& msg) {
    core::types::AttitudeData data;
    Q_UNUSED(msg)
    data.timestamp = QDateTime::currentDateTimeUtc();
    m_state->updateAttitude(data);
    m_bus->emitAttitudeChanged(data);
}

void MavlinkParser::parseGlobalPosition(const types::MavlinkMessage& msg) {
    core::types::PositionData data;
    Q_UNUSED(msg)
    data.timestamp = QDateTime::currentDateTimeUtc();
    m_state->updatePosition(data);
    m_bus->emitPositionChanged(data);
}

void MavlinkParser::parseBatteryStatus(const types::MavlinkMessage& msg) {
    core::types::BatteryData data;
    Q_UNUSED(msg)
    m_state->updateBattery(data);
    m_bus->emitBatteryChanged(data);
}

void MavlinkParser::parseMissionCurrent(const types::MavlinkMessage& msg) {
    int index = 0;
    Q_UNUSED(msg)
    m_state->updateMissionCurrent(index);
    m_bus->emitMissionCurrentChanged(index);
}

void MavlinkParser::parseStatusText(const types::MavlinkMessage& msg) {
    Q_UNUSED(msg)
    m_bus->emitStatusText("STATUS_TEXT placeholder");
}

} // namespace aegis::telemetry
