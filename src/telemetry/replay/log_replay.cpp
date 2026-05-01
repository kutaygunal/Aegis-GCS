#include "log_replay.hpp"
#include "core/bus/telemetry_bus.hpp"
#include "parsers.hpp"
#include <QDataStream>
#include <QDebug>

namespace aegis::telemetry {

LogReplay::LogReplay(TelemetryBus* bus, QObject* parent)
    : QObject(parent), m_bus(bus) {
    m_timer = new QTimer(this);
    m_timer->setInterval(10);  // 100 Hz tick for fine-grained timing
    connect(m_timer, &QTimer::timeout, this, &LogReplay::onTick);
}

LogReplay::~LogReplay() {
    m_file.close();
}

bool LogReplay::loadFile(const QString& path) {
    m_file.setFileName(path);
    if (!m_file.open(QIODevice::ReadOnly)) {
        qWarning() << "[LogReplay] Failed to open" << path;
        return false;
    }
    m_totalBytes = m_file.size();
    return true;
}

void LogReplay::start(qreal speed) {
    m_speed = speed;
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
    // TODO: parse and emit exactly one message
}

void LogReplay::seek(qreal percent) {
    if (!m_file.isOpen()) return;
    qint64 pos = static_cast<qint64>(m_totalBytes * percent);
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
    // TODO: parse MAVLink frame, timestamp-interpolate, emit via MavlinkParser
    emit progressChanged(progress());
}

} // namespace aegis::telemetry
