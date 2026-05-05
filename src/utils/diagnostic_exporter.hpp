#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace aegis::utils {

/**
 * @brief Service for exporting diagnostic bundles (ZIP of logs, config, manifest).
 *
 * Can be called synchronously or asynchronously. The async path runs in a
 * background thread and emits the result on the main thread via Qt signals.
 */
class DiagnosticExporter : public QObject {
    Q_OBJECT

public:
    explicit DiagnosticExporter(const QString& logFilePath,
                                int logMaxFiles,
                                const QString& configPath,
                                QObject* parent = nullptr);

    /** @brief Synchronous export. Returns true on success. */
    bool exportBundle(const QString& outputPath, QString* error = nullptr);

    /** @brief Asynchronous export; emits exportFinished when done. */
    void exportBundleAsync(const QString& outputPath);

public slots:
    /** @brief Slot for TelemetryBus-triggered export. */
    void onExportRequested(const QString& outputPath);

signals:
    void exportFinished(bool success, const QString& path, const QString& error);

private:
    bool collectFiles(QStringList& outFiles, QString& outManifestData);
    static QString gitRevision();
    static QString manifestToJson(const QString& gitRev,
                                  const QStringList& files,
                                  const QList<qint64>& sizes);

    QString m_logFilePath;
    int m_logMaxFiles;
    QString m_configPath;
};

} // namespace aegis::utils
