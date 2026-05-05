#include "logging.hpp"
#include <QCoreApplication>
#include <QThread>
#include <QFileInfo>
#include <QDir>

namespace aegis::utils {

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

Logger::~Logger() {
    if (m_file.isOpen()) m_file.close();
}

void Logger::setLogFile(const QString& path) {
    QMutexLocker lock(&m_mutex);
    if (m_file.isOpen()) m_file.close();
    m_file.setFileName(path);
    if (!path.isEmpty()) {
        m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
        m_stream.setDevice(&m_file);
    }
}

void Logger::setMinLevel(LogLevel level) {
    QMutexLocker lock(&m_mutex);
    m_minLevel = level;
}

void Logger::setRotation(qint64 maxSizeBytes, int maxFiles) {
    QMutexLocker lock(&m_mutex);
    m_maxSizeBytes = maxSizeBytes;
    m_maxFiles = maxFiles;
}

void Logger::maybeRotate() {
    if (m_maxSizeBytes <= 0 || m_maxFiles <= 0) return;
    if (!m_file.isOpen()) return;
    if (m_file.size() < m_maxSizeBytes) return;

    m_file.close();
    rotateFiles();
    m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    m_stream.setDevice(&m_file);
}

void Logger::rotateFiles() {
    QString base = m_file.fileName();
    if (base.isEmpty()) return;

    // Remove the oldest backup that would be pushed out of range.
    QString oldest = QString("%1.%2").arg(base).arg(m_maxFiles - 1);
    if (QFile::exists(oldest)) QFile::remove(oldest);

    // Shift backups upward: .(N-2) -> .(N-1) ... .1 -> .2
    for (int i = m_maxFiles - 2; i >= 1; --i) {
        QString older = QString("%1.%2").arg(base).arg(i);
        QString newer = QString("%1.%2").arg(base).arg(i + 1);
        if (QFile::exists(older)) {
            if (QFile::exists(newer)) QFile::remove(newer);
            QFile::rename(older, newer);
        }
    }

    QString firstBackup = QString("%1.1").arg(base);
    if (QFile::exists(firstBackup)) QFile::remove(firstBackup);
    QFile::rename(base, firstBackup);
}

void Logger::log(LogLevel level, const QString& category, const QString& message) {
    if (level < m_minLevel) return;

    const char* levelStr = "?";
    switch (level) {
        case LogLevel::Debug:    levelStr = "DBG"; break;
        case LogLevel::Info:     levelStr = "INF"; break;
        case LogLevel::Warning:  levelStr = "WRN"; break;
        case LogLevel::Error:    levelStr = "ERR"; break;
        case LogLevel::Critical: levelStr = "CRT"; break;
    }

    QString threadId = QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()), 16).toUpper();

    QString entry = QString("[%1] [%2] {%3} %4: %5")
                        .arg(QDateTime::currentDateTime().toString(Qt::ISODate),
                             threadId, levelStr, category, message);

    QMutexLocker lock(&m_mutex);
    qDebug().noquote() << entry;
    if (m_file.isOpen()) {
        m_stream << entry << Qt::endl;
        m_stream.flush();
        maybeRotate();
    }
}

} // namespace aegis::utils
