#include "mavlink_io.hpp"
#include "types/mavlink_types.hpp"
#include "core/types/common.hpp"
#include <mavlink.h>
#include <QDebug>
#include <QNetworkDatagram>
#include <QMetaObject>

namespace aegis::telemetry {

MavlinkIO::MavlinkIO(QObject* parent) : QObject(parent) {
    qRegisterMetaType<types::MavlinkMessage>("aegis::telemetry::types::MavlinkMessage");
    moveToThread(&m_workerThread);
    connect(&m_workerThread, &QThread::started, this, &MavlinkIO::setupSocket);
}

MavlinkIO::~MavlinkIO() {
    stop();
    m_workerThread.wait(5000);
}

ConnectionStateMachine::State MavlinkIO::connectionState() const {
    return m_stateMachine ? m_stateMachine->currentState()
                          : ConnectionStateMachine::State::Disconnected;
}

void MavlinkIO::start(const QHostAddress& bindAddress, quint16 bindPort) {
    if (m_running) {
        qWarning() << "[MavlinkIO] Already running";
        return;
    }

    m_bindAddress = bindAddress;
    m_bindPort = bindPort;
    m_remoteKnown = false;
    m_firstPacketReceived = false;
    m_lastHeartbeatUtc = QDateTime();
    m_workerThread.start(QThread::TimeCriticalPriority);
}

void MavlinkIO::stop() {
    if (!m_running && !m_workerThread.isRunning()) {
        return;
    }

    if (m_workerThread.isRunning()) {
        QMetaObject::invokeMethod(this, [this]() {
            if (m_heartbeatTimer) m_heartbeatTimer->stop();
            if (m_txTimer) m_txTimer->stop();
            if (m_socket) {
                m_socket->close();
                m_socket->deleteLater();
                m_socket = nullptr;
            }
            if (m_stateMachine) {
                m_stateMachine->onStop();
            }
            m_remoteKnown = false;
            m_firstPacketReceived = false;
            m_running = false;
        }, Qt::BlockingQueuedConnection);
    } else {
        m_running = false;
        m_remoteKnown = false;
        m_firstPacketReceived = false;
        if (m_stateMachine) {
            m_stateMachine->onStop();
        }
    }

    m_workerThread.quit();
    m_workerThread.wait(2000);
}

void MavlinkIO::setHeartbeatTimeoutMs(int timeoutMs) {
    m_heartbeatTimeoutMs = qMax(1, timeoutMs);
    if (m_heartbeatTimer) {
        m_heartbeatTimer->setInterval(m_heartbeatTimeoutMs);
    }
}

void MavlinkIO::sendMessage(const types::MavlinkMessage& msg) {
    QMutexLocker lock(&m_txMutex);
    m_txQueue.enqueue(msg);
}

void MavlinkIO::sendCommand(const aegis::core::types::VehicleCommand& cmd) {
    mavlink_message_t msg{};
    constexpr uint8_t kSourceSystemId = 255;
    constexpr uint8_t kSourceComponentId = MAV_COMP_ID_MISSIONPLANNER;
    mavlink_msg_command_long_pack(
        kSourceSystemId,
        kSourceComponentId,
        &msg,
        cmd.targetSystem,
        cmd.targetComponent,
        cmd.commandId,
        0,  // confirmation
        cmd.params[0], cmd.params[1], cmd.params[2], cmd.params[3],
        cmd.params[4], cmd.params[5], cmd.params[6]);

    types::MavlinkMessage out;
    out.raw = msg;
    out.timestamp = QDateTime::currentDateTimeUtc();

    QMutexLocker lock(&m_txMutex);
    m_txQueue.enqueue(out);
}

void MavlinkIO::setRemoteAddress(const QHostAddress& addr, quint16 port) {
    QMutexLocker<QMutex> lock(&m_txMutex);
    m_remoteAddress = addr;
    m_remotePort = port;
    m_remoteKnown = true;
    qDebug() << "[MavlinkIO] Remote target set to" << addr.toString() << ":" << port;
}

void MavlinkIO::setupSocket() {
    m_socket = new QUdpSocket(this);
    if (!m_socket->bind(m_bindAddress, m_bindPort)) {
        m_running = false;
        emit parseError(m_socket->errorString());
        m_workerThread.quit();
        return;
    }

    connect(m_socket, &QUdpSocket::readyRead, this, &MavlinkIO::onReadyRead);

    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(m_heartbeatTimeoutMs);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &MavlinkIO::onHeartbeatTimeout);
    m_heartbeatTimer->start();

    m_txTimer = new QTimer(this);
    m_txTimer->setInterval(20);
    connect(m_txTimer, &QTimer::timeout, this, &MavlinkIO::processOutboundQueue);
    m_txTimer->start();

    // Create state machine on the worker thread
    m_stateMachine = new ConnectionStateMachine(this);
    connect(m_stateMachine, &ConnectionStateMachine::stateChanged,
            this, [this](ConnectionStateMachine::State state) {
        emit connectionStateChanged(
            ConnectionStateMachine::stateToConnectionState(state));
    });

    m_running = true;
    m_firstPacketReceived = false;

    m_stateMachine->onSocketBound();

    qDebug() << "[MavlinkIO] Listening on" << m_bindAddress.toString() << ":"
             << m_bindPort;
}

void MavlinkIO::onReadyRead() {
    static mavlink_status_t status;
    static mavlink_message_t msg;

    while (m_socket->hasPendingDatagrams()) {
        QNetworkDatagram dgram = m_socket->receiveDatagram();
        const QByteArray data = dgram.data();

        // Auto-detect remote address from first incoming datagram
        if (!m_remoteKnown && !dgram.senderAddress().isNull()) {
            setRemoteAddress(dgram.senderAddress(), dgram.senderPort());
        }

        // Notify state machine on first packet
        if (!m_firstPacketReceived && m_stateMachine) {
            m_firstPacketReceived = true;
            m_stateMachine->onPacketReceived();
        }

        for (char c : data) {
            if (mavlink_parse_char(MAVLINK_COMM_0,
                                   static_cast<uint8_t>(c),
                                   &msg, &status)) {
                if (msg.msgid == MAVLINK_MSG_ID_HEARTBEAT) {
                    m_lastHeartbeatUtc = QDateTime::currentDateTimeUtc();
                    if (m_stateMachine) {
                        m_stateMachine->onHeartbeatReceived();
                    }
                }

                types::MavlinkMessage out;
                out.raw = msg;
                out.timestamp = QDateTime::currentDateTimeUtc();
                emit messageReceived(out);
            }
        }
    }
}

void MavlinkIO::onHeartbeatTimeout() {
    if (!m_lastHeartbeatUtc.isValid()) {
        return;
    }

    if (m_lastHeartbeatUtc.msecsTo(QDateTime::currentDateTimeUtc()) >= m_heartbeatTimeoutMs) {
        if (m_stateMachine) {
            m_stateMachine->onHeartbeatTimeout();
        }
        qWarning() << "[MavlinkIO] Heartbeat timeout after" << m_heartbeatTimeoutMs << "ms";
    }
}

void MavlinkIO::processOutboundQueue() {
    QMutexLocker lock(&m_txMutex);
    if (!m_socket || !m_socket->isOpen()) return;

    while (!m_txQueue.isEmpty()) {
        const auto msg = m_txQueue.dequeue();
        uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
        uint16_t len = mavlink_msg_to_send_buffer(buffer, &msg.raw);

        if (m_remoteKnown) {
            qint64 sent = m_socket->writeDatagram(
                reinterpret_cast<const char*>(buffer), len,
                m_remoteAddress, m_remotePort);
            if (sent < 0) {
                qWarning() << "[MavlinkIO] TX failed:" << m_socket->errorString();
            }
        } else {
            // Broadcast to link-local if no specific remote known
            m_socket->writeDatagram(
                reinterpret_cast<const char*>(buffer), len,
                QHostAddress::Broadcast, m_bindPort);
        }
    }
}

} // namespace aegis::telemetry
