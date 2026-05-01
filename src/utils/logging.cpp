#include "logging.hpp"
#include <QCoreApplication>

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
    m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    m_stream.setDevice(&m_file);
}

void Logger::setMinLevel(LogLevel level) {
    QMutexLocker lock(&m_mutex);
    m_minLevel = level;
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

    QString entry = QString("[%1] {%2} %3: %4")
                        .arg(QDateTime::currentDateTime().toString(Qt::ISODate),
                             levelStr, category, message);

    QMutexLocker lock(&m_mutex);
    qDebug().noquote() << entry;
    if (m_file.isOpen()) {
        m_stream << entry << Qt::endl;
        m_stream.flush();
    }
}

} // namespace aegis::utils
