#pragma once

#include <QString>
#include <QUuid>
#include <QDateTime>
#include <QMetaType>

namespace aegis::core::types {

enum class AlertLevel {
    Info,
    Warning,
    Critical,
    Emergency
};

enum class SystemStatus {
    Unknown,
    Booting,
    Calibrating,
    Standby,
    Active,
    Critical,
    Emergency,
    PowerOff
};

enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Error
};

struct VehicleCommand {
    quint16 commandId{0};
    quint8 targetSystem{1};
    quint8 targetComponent{1};
    float params[7]{};
};

struct AttitudeData {
    qreal roll{0.0};        // rad
    qreal pitch{0.0};
    qreal yaw{0.0};
    qreal rollSpeed{0.0};   // rad/s
    qreal pitchSpeed{0.0};
    qreal yawSpeed{0.0};
    QDateTime timestamp;
};

struct PositionData {
    qint32 lat{0};         // degE7
    qint32 lon{0};         // degE7
    qint32 alt{0};         // mm MSL
    qint32 relativeAlt{0}; // mm above home
    qint16 vx{0};          // cm/s
    qint16 vy{0};
    qint16 vz{0};
    quint16 hdg{0};        // cdeg
    QDateTime timestamp;
};

struct BatteryData {
    qint8 remaining{-1};  // -1 = unknown
    qreal voltageV{0.0};
    qreal currentA{0.0};
};

struct SystemState {
    quint8 systemId{0};
    quint8 componentId{0};
    quint8 baseMode{0};
    quint32 customMode{0};
    SystemStatus status{SystemStatus::Unknown};
    bool armed{false};
};

} // namespace aegis::core::types

Q_DECLARE_METATYPE(aegis::core::types::AttitudeData)
Q_DECLARE_METATYPE(aegis::core::types::PositionData)
Q_DECLARE_METATYPE(aegis::core::types::BatteryData)
Q_DECLARE_METATYPE(aegis::core::types::SystemState)
Q_DECLARE_METATYPE(aegis::core::types::VehicleCommand)
Q_DECLARE_METATYPE(aegis::core::types::AlertLevel)
