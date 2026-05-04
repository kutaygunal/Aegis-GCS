#pragma once

#include <QObject>
#include "types/mavlink_types.hpp"
#include "core/types/common.hpp"

namespace aegis::core {
    class TelemetryBus;
    class VehicleState;
}

namespace aegis::telemetry {

/**
 * @brief Demultiplexes MAVLink messages into canonical VehicleState updates.
 *
 * Runs on the main thread (fed by MavlinkIO via QueuedConnection).
 * Handles dialect detection, message validation, and rate decoupling.
 */
class MavlinkParser : public QObject {
    Q_OBJECT

public:
    explicit MavlinkParser(aegis::core::TelemetryBus* bus,
                           aegis::core::VehicleState* state,
                           QObject* parent = nullptr);

    void processMessage(const types::MavlinkMessage& msg);

private:
    aegis::core::TelemetryBus* m_bus;
    aegis::core::VehicleState* m_state;

    void parseHeartbeat(const mavlink_message_t& msg);
    void parseAttitude(const mavlink_message_t& msg);
    void parseGlobalPosition(const mavlink_message_t& msg);
    void parseBatteryStatus(const mavlink_message_t& msg);
    void parseMissionCurrent(const mavlink_message_t& msg);
    void parseStatusText(const mavlink_message_t& msg);

    static core::types::SystemStatus mapMavState(uint8_t mavState);
    static core::types::AlertLevel mapSeverity(uint8_t severity);
};

} // namespace aegis::telemetry
