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

    void parseHeartbeat(const types::MavlinkMessage& msg);
    void parseAttitude(const types::MavlinkMessage& msg);
    void parseGlobalPosition(const types::MavlinkMessage& msg);
    void parseBatteryStatus(const types::MavlinkMessage& msg);
    void parseMissionCurrent(const types::MavlinkMessage& msg);
    void parseStatusText(const types::MavlinkMessage& msg);
};

} // namespace aegis::telemetry
