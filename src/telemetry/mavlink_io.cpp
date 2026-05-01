#include "mavlink_io.hpp"
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
    m_txTimer->setInterval(20);  // 50 Hz max TX
    connect(m_txTimer, &QTimer::timeout, this, &MavlinkIO::processOutboundQueue);
    m_txTimer->start();

    m_running = true;
    emit connectionStateChanged(true);
    qDebug() << "[MavlinkIO] Listening on" << m_bindAddress.toString() << ":"
             << m_bindPort;
}

void MavlinkIO::onReadyRead() {
    while (m_socket->hasPendingDatagrams()) {
        QNetworkDatagram dgram = m_socket->receiveDatagram();
        types::MavlinkMessage msg;
        msg.timestamp = QDateTime::currentDateTimeUtc();
        msg.payload = dgram.data();
        // TODO: parse sysid/compid/msgid from MAVLink framing
        emit messageReceived(msg);
    }
}

void MavlinkIO::onHeartbeatTimeout() {
    // If no heartbeat received in 3s, mark disconnected
    if (m_running) {
        // TODO: implement actual timeout tracking per-system
    }
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
