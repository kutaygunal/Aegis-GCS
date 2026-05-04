#include "mavlink_io.hpp"
#include "types/mavlink_types.hpp"
#include <mavlink.h>
#include <QDebug>
#include <QNetworkDatagram>

namespace aegis::telemetry {

MavlinkIO::MavlinkIO(QObject* parent) : QObject(parent) {
    moveToThread(&m_workerThread);
    connect(&m_workerThread, &QThread::started, this, &MavlinkIO::setupSocket);
}

MavlinkIO::~MavlinkIO() {
    stop();
    m_workerThread.wait(5000);
}

void MavlinkIO::start(const QHostAddress& bindAddress, quint16 bindPort) {
    m_bindAddress = bindAddress;
    m_bindPort = bindPort;
    m_workerThread.start(QThread::TimeCriticalPriority);
}

void MavlinkIO::stop() {
    m_running = false;
    m_workerThread.quit();
}

void MavlinkIO::sendMessage(const types::MavlinkMessage& msg) {
    QMutexLocker lock(&m_txMutex);
    m_txQueue.enqueue(msg);
}

void MavlinkIO::setupSocket() {
    m_socket = new QUdpSocket(this);
    if (!m_socket->bind(m_bindAddress, m_bindPort)) {
        emit parseError(m_socket->errorString());
        m_workerThread.quit();
        return;
    }

    connect(m_socket, &QUdpSocket::readyRead, this, &MavlinkIO::onReadyRead);

    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(3000);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &MavlinkIO::onHeartbeatTimeout);
    m_heartbeatTimer->start();

    m_txTimer = new QTimer(this);
    m_txTimer->setInterval(20);
    connect(m_txTimer, &QTimer::timeout, this, &MavlinkIO::processOutboundQueue);
    m_txTimer->start();

    m_running = true;
    emit connectionStateChanged(true);
    qDebug() << "[MavlinkIO] Listening on" << m_bindAddress.toString() << ":"
             << m_bindPort;
}

void MavlinkIO::onReadyRead() {
    static mavlink_status_t status;
    static mavlink_message_t msg;

    while (m_socket->hasPendingDatagrams()) {
        QNetworkDatagram dgram = m_socket->receiveDatagram();
        const QByteArray data = dgram.data();

        for (char c : data) {
            if (mavlink_parse_char(MAVLINK_COMM_0,
                                   static_cast<uint8_t>(c),
                                   &msg, &status)) {
                types::MavlinkMessage out;
                out.raw = msg;
                out.timestamp = QDateTime::currentDateTimeUtc();
                emit messageReceived(out);
            }
        }
    }
}

void MavlinkIO::onHeartbeatTimeout() {
    // TODO: implement actual timeout tracking per-system
}

void MavlinkIO::processOutboundQueue() {
    QMutexLocker lock(&m_txMutex);
    while (!m_txQueue.isEmpty()) {
        const auto msg = m_txQueue.dequeue();
        // TODO: re-encode MAVLink frame and broadcast
        Q_UNUSED(msg)
    }
}

} // namespace aegis::telemetry
