#pragma once

#include <QDateTime>
#include <mavlink.h>

namespace aegis::telemetry::types {

/**
 * @brief MAVLink message envelope carrying the parsed C library struct.
 *
 * The mavlink_message_t struct is safely copyable (plain C data)
 * and carries all fields needed for decoding via mavlink_msg_*_decode().
 */
struct MavlinkMessage {
    mavlink_message_t raw{};
    QDateTime timestamp;

    uint8_t sysid() const    { return raw.sysid; }
    uint8_t compid() const   { return raw.compid; }
    uint32_t msgid() const     { return raw.msgid; }
};

} // namespace aegis::telemetry::types

Q_DECLARE_METATYPE(aegis::telemetry::types::MavlinkMessage)
