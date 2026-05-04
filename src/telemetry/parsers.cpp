#include "parsers.hpp"
#include "core/bus/telemetry_bus.hpp"
#include "core/state/vehicle_state.hpp"
#include <mavlink.h>
#include <QDebug>

namespace aegis::telemetry {

MavlinkParser::MavlinkParser(aegis::core::TelemetryBus* bus,
                             aegis::core::VehicleState* state,
                             QObject* parent)
    : QObject(parent), m_bus(bus), m_state(state) {}

void MavlinkParser::processMessage(const types::MavlinkMessage& msg) {
    const uint32_t msgid = msg.msgid();
    switch (msgid) {
        case MAVLINK_MSG_ID_HEARTBEAT:
            parseHeartbeat(msg.raw);
            break;
        case MAVLINK_MSG_ID_ATTITUDE:
            parseAttitude(msg.raw);
            break;
        case MAVLINK_MSG_ID_GLOBAL_POSITION_INT:
            parseGlobalPosition(msg.raw);
            break;
        case MAVLINK_MSG_ID_BATTERY_STATUS:
            parseBatteryStatus(msg.raw);
            break;
        case MAVLINK_MSG_ID_MISSION_CURRENT:
            parseMissionCurrent(msg.raw);
            break;
        case MAVLINK_MSG_ID_STATUSTEXT:
            parseStatusText(msg.raw);
            break;
        default:
            // Unhandled message type — no-op
            break;
    }
}

void MavlinkParser::parseHeartbeat(const mavlink_message_t& msg) {
    mavlink_heartbeat_t heartbeat;
    mavlink_msg_heartbeat_decode(&msg, &heartbeat);

    core::types::SystemState state;
    state.systemId  = msg.sysid;
    state.componentId = msg.compid;
    state.baseMode  = heartbeat.base_mode;
    state.customMode = heartbeat.custom_mode;
    state.status    = mapMavState(heartbeat.system_status);
    state.armed     = (heartbeat.base_mode & MAV_MODE_FLAG_SAFETY_ARMED) != 0;

    m_state->updateSystemState(state);
    m_bus->emitHeartbeat(state);
}

void MavlinkParser::parseAttitude(const mavlink_message_t& msg) {
    mavlink_attitude_t attitude;
    mavlink_msg_attitude_decode(&msg, &attitude);

    core::types::AttitudeData data;
    data.roll      = attitude.roll;        // rad
    data.pitch     = attitude.pitch;
    data.yaw       = attitude.yaw;
    data.rollSpeed = attitude.rollspeed;    // rad/s
    data.pitchSpeed = attitude.pitchspeed;
    data.yawSpeed  = attitude.yawspeed;
    data.timestamp = QDateTime::currentDateTimeUtc();

    m_state->updateAttitude(data);
    m_bus->emitAttitudeChanged(data);
}

void MavlinkParser::parseGlobalPosition(const mavlink_message_t& msg) {
    mavlink_global_position_int_t pos;
    mavlink_msg_global_position_int_decode(&msg, &pos);

    core::types::PositionData data;
    data.lat         = pos.lat;          // degE7
    data.lon         = pos.lon;          // degE7
    data.alt         = pos.alt;          // mm MSL
    data.relativeAlt = pos.relative_alt; // mm above home
    data.vx          = pos.vx;           // cm/s
    data.vy          = pos.vy;
    data.vz          = pos.vz;
    data.hdg         = pos.hdg;          // cdeg
    data.timestamp   = QDateTime::currentDateTimeUtc();

    m_state->updatePosition(data);
    m_bus->emitPositionChanged(data);
}

void MavlinkParser::parseBatteryStatus(const mavlink_message_t& msg) {
    mavlink_battery_status_t bat;
    mavlink_msg_battery_status_decode(&msg, &bat);

    core::types::BatteryData data;
    data.remaining = static_cast<qint8>(bat.battery_remaining); // -1 = unknown
    // MAVLink sends in cV; convert to V
    data.voltageV = (bat.voltages[0] == UINT16_MAX)
                        ? 0.0f
                        : static_cast<qreal>(bat.voltages[0]) / 100.0f;
    // Current is in cA (10 mA steps), -1 = unknown
    data.currentA = (bat.current_battery == -1)
                        ? 0.0f
                        : static_cast<qreal>(bat.current_battery) / 100.0f;

    m_state->updateBattery(data);
    m_bus->emitBatteryChanged(data);
}

void MavlinkParser::parseMissionCurrent(const mavlink_message_t& msg) {
    mavlink_mission_current_t current;
    mavlink_msg_mission_current_decode(&msg, &current);

    m_state->updateMissionCurrent(static_cast<int>(current.seq));
    m_bus->emitMissionCurrentChanged(static_cast<int>(current.seq));
}

void MavlinkParser::parseStatusText(const mavlink_message_t& msg) {
    mavlink_statustext_t text;
    mavlink_msg_statustext_decode(&msg, &text);

    QString message = QString::fromUtf8(text.text);
    core::types::AlertLevel level = mapSeverity(text.severity);

    m_bus->emitAlert(level, message);
}

core::types::SystemStatus MavlinkParser::mapMavState(uint8_t mavState) {
    switch (mavState) {
        case MAV_STATE_UNINIT:       return core::types::SystemStatus::Unknown;
        case MAV_STATE_BOOT:         return core::types::SystemStatus::Booting;
        case MAV_STATE_CALIBRATING:  return core::types::SystemStatus::Calibrating;
        case MAV_STATE_STANDBY:      return core::types::SystemStatus::Standby;
        case MAV_STATE_ACTIVE:       return core::types::SystemStatus::Active;
        case MAV_STATE_CRITICAL:     return core::types::SystemStatus::Critical;
        case MAV_STATE_EMERGENCY:    return core::types::SystemStatus::Emergency;
        case MAV_STATE_POWEROFF:     return core::types::SystemStatus::PowerOff;
        default:                     return core::types::SystemStatus::Unknown;
    }
}

core::types::AlertLevel MavlinkParser::mapSeverity(uint8_t severity) {
    switch (severity) {
        case MAV_SEVERITY_EMERGENCY:
        case MAV_SEVERITY_ALERT:
            return core::types::AlertLevel::Emergency;
        case MAV_SEVERITY_CRITICAL:
            return core::types::AlertLevel::Critical;
        case MAV_SEVERITY_ERROR:
            return core::types::AlertLevel::Critical;
        case MAV_SEVERITY_WARNING:
        case MAV_SEVERITY_NOTICE:
            return core::types::AlertLevel::Warning;
        case MAV_SEVERITY_INFO:
        case MAV_SEVERITY_DEBUG:
        default:
            return core::types::AlertLevel::Info;
    }
}

} // namespace aegis::telemetry
