#pragma once

#include <QObject>
#include <QFile>
#include <QTimer>
#include <QDateTime>

namespace aegis::core { class TelemetryBus; }

namespace aegis::telemetry {

/**
 * @brief Replays binary MAVLink log files (.tlog) into the TelemetryBus.
 *
 * Supports configurable speed (1x, 2x, 10x) or step mode for debugging.
 */
class LogReplay : public QObject {
    Q_OBJECT

public:
    explicit LogReplay(aegis::core::TelemetryBus* bus, QObject* parent = nullptr);
    ~LogReplay() override;

    bool loadFile(const QString& path);
    void start(qreal speed = 1.0);
    void pause();
    void step();  // advance one message in paused mode
    void seek(qreal percent);  // 0.0 - 1.0

    bool isPlaying() const { return m_playing; }
    qreal progress() const;

signals:
    void playbackStarted();
    void playbackPaused();
    void playbackFinished();
    void progressChanged(qreal percent);

private slots:
    void onTick();

private:
    aegis::core::TelemetryBus* m_bus;
    QFile m_file;
    QTimer* m_timer{nullptr};
    bool m_playing{false};
    qreal m_speed{1.0};
    qint64 m_totalBytes{0};
};

} // namespace aegis::telemetry
