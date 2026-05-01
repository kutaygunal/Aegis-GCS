#pragma once

#include <QDateTime>

namespace aegis::telemetry::types {

/**
 * @brief Normalized MAVLink message envelope.
 *
 * Decouples the rest of the system from the concrete MAVLink dialect,
 * enabling SIL/HIL log replay and future DDS/ZeroMQ backends.
 */
struct MavlinkMessage {
    quint8  sysid{0};
    quint8  compid{0};
    quint32 msgid{0};
    QDateTime timestamp;
    QByteArray payload;
};

} // namespace aegis::telemetry::types
