#pragma once

#include <QObject>
#include <QThread>
#include <QUdpSocket>
#include <QHostAddress>
#include <QTimer>
#include <QQueue>
#include <QMutex>
#include <QDateTime>
#include "types/mavlink_types.hpp"
#include "core/types/common.hpp"
#include "connection_state_machine.hpp"

namespace aegis::telemetry {

/**
 * @brief Dedicated I/O thread for MAVLink UDP reception and transmission.
 *
 * All socket operations run on a background QThread. Parsed messages are
 * emitted via QueuedConnection to the TelemetryBus living on the main thread.
 */
class MavlinkIO : public QObject {
    Q_OBJECT

public:
    explicit MavlinkIO(QObject* parent = nullptr);
    ~MavlinkIO() override;

    void start(const QHostAddress& bindAddress = QHostAddress::Any,
               quint16 bindPort = 14550);
    void stop();
    void setHeartbeatTimeoutMs(int timeoutMs);

    bool isRunning() const { return m_running; }
    aegis::telemetry::ConnectionStateMachine::State connectionState() const;

    /** @brief Enqueue a MAVLink message for transmission. Thread-safe. */
    void sendMessage(const types::MavlinkMessage& msg);

    /** @brief Enqueue a normalized vehicle command for MAVLink encoding. Thread-safe. */
    void sendCommand(const aegis::core::types::VehicleCommand& cmd);

    /** @brief Set the remote target address for outbound messages (auto-detected from first RX). */
    void setRemoteAddress(const QHostAddress& addr, quint16 port = 14550);

signals:
    void messageReceived(const aegis::telemetry::types::MavlinkMessage& msg);
    void connectionStateChanged(aegis::core::types::ConnectionState state);
    void parseError(const QString& reason);

private slots:
    void onReadyRead();
    void onHeartbeatTimeout();
    void processOutboundQueue();

private:
    void setupSocket();

    QThread m_workerThread;
    QUdpSocket* m_socket{nullptr};
    QTimer* m_heartbeatTimer{nullptr};
    QTimer* m_txTimer{nullptr};
    bool m_running{false};

    ConnectionStateMachine* m_stateMachine{nullptr};

    QMutex m_txMutex;
    QQueue<types::MavlinkMessage> m_txQueue;

    QHostAddress m_remoteAddress;
    quint16 m_remotePort{14550};
    bool m_remoteKnown{false};

    QHostAddress m_bindAddress;
    quint16 m_bindPort{14550};
    int m_heartbeatTimeoutMs{3000};
    QDateTime m_lastHeartbeatUtc;

    bool m_firstPacketReceived{false};
};

} // namespace aegis::telemetry
