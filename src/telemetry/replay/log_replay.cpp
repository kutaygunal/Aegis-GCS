#include "log_replay.hpp"
#include "core/bus/telemetry_bus.hpp"
#include <mavlink.h>
#include <QDebug>

namespace aegis::telemetry {

LogReplay::LogReplay(aegis::core::TelemetryBus* bus, QObject* parent)
    : QObject(parent), m_bus(bus) {
    m_timer = new QTimer(this);
    m_timer->setInterval(10);  // 100 Hz tick for fine-grained timing
    connect(m_timer, &QTimer::timeout, this, &LogReplay::onTick);
}

LogReplay::~LogReplay() {
    m_file.close();
}

bool LogReplay::loadFile(const QString& path) {
    if (m_file.isOpen()) {
        m_file.close();
    }
    m_file.setFileName(path);
    if (!m_file.open(QIODevice::ReadOnly)) {
        qWarning() << "[LogReplay] Failed to open" << path;
        return false;
    }
    m_totalBytes = m_file.size();
    m_file.seek(0);
    return true;
}

void LogReplay::start(qreal speed) {
    if (!m_file.isOpen()) {
        qWarning() << "[LogReplay] No replay file loaded";
        return;
    }
    m_speed = qMax<qreal>(0.1, speed);
    m_playing = true;
    m_timer->start();
    emit playbackStarted();
}

void LogReplay::pause() {
    m_playing = false;
    m_timer->stop();
    emit playbackPaused();
}

void LogReplay::step() {
    if (!m_file.isOpen()) return;

    types::MavlinkMessage msg;
    if (readNextMessage(msg)) {
        emit messageReplayed(msg);
        emit progressChanged(progress());
    }
}

void LogReplay::seek(qreal percent) {
    if (!m_file.isOpen()) return;
    const qreal clamped = qBound<qreal>(0.0, percent, 1.0);
    qint64 pos = static_cast<qint64>(m_totalBytes * clamped);
    m_file.seek(pos);
}

qreal LogReplay::progress() const {
    if (m_totalBytes == 0) return 0.0;
    return static_cast<qreal>(m_file.pos()) / static_cast<qreal>(m_totalBytes);
}

void LogReplay::onTick() {
    if (!m_playing || m_file.atEnd()) {
        pause();
        emit playbackFinished();
        return;
    }

    const int messagesPerTick = qMax(1, static_cast<int>(m_speed));
    bool replayedAny = false;
    for (int i = 0; i < messagesPerTick; ++i) {
        types::MavlinkMessage msg;
        if (!readNextMessage(msg)) {
            pause();
            emit playbackFinished();
            return;
        }
        replayedAny = true;
        emit messageReplayed(msg);
    }

    if (replayedAny) {
        emit progressChanged(progress());
    }
}

bool LogReplay::readNextMessage(types::MavlinkMessage& out) {
    if (!m_file.isOpen()) return false;

    mavlink_status_t status{};
    mavlink_message_t msg{};
    char byte = 0;

    while (!m_file.atEnd()) {
        if (m_file.read(&byte, 1) != 1) {
            break;
        }

        if (mavlink_parse_char(MAVLINK_COMM_0,
                               static_cast<uint8_t>(byte),
                               &msg, &status)) {
            out.raw = msg;
            out.timestamp = QDateTime::currentDateTimeUtc();
            return true;
        }
    }

    return false;
}

} // namespace aegis::telemetry
