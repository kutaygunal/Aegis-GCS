#pragma once

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>
#include <QDebug>

namespace aegis::utils {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

/**
 * @brief Thread-safe structured logging sink.
 *
 * Writes to both stdout and a rotating log file.
 */
class Logger {
public:
    static Logger& instance();

    void setLogFile(const QString& path);
    void setMinLevel(LogLevel level);

    void log(LogLevel level, const QString& category, const QString& message);

private:
    Logger() = default;
    ~Logger();

    QMutex m_mutex;
    QFile m_file;
    QTextStream m_stream;
    LogLevel m_minLevel{LogLevel::Debug};
};

#define AEGIS_LOG(level, cat, msg) \
    aegis::utils::Logger::instance().log(level, cat, msg)

} // namespace aegis::utils
