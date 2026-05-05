#include "diagnostic_exporter.hpp"
#include "simple_zip_writer.hpp"
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDateTime>
#include <QProcess>
#include <QDir>
#include <QtConcurrent>
#include <QMetaObject>
#include <QDebug>

namespace aegis::utils {

DiagnosticExporter::DiagnosticExporter(const QString& logFilePath,
                                         int logMaxFiles,
                                         const QString& configPath,
                                         QObject* parent)
    : QObject(parent)
    , m_logFilePath(logFilePath)
    , m_logMaxFiles(logMaxFiles)
    , m_configPath(configPath) {
}

bool DiagnosticExporter::exportBundle(const QString& outputPath, QString* error) {
    QStringList files;
    QString manifest;
    if (!collectFiles(files, manifest)) {
        if (error) *error = QStringLiteral("Failed to collect diagnostic files");
        return false;
    }

    SimpleZipWriter writer(outputPath);
    if (!writer.isOpen()) {
        if (error) *error = QStringLiteral("Cannot open ZIP for writing: %1").arg(outputPath);
        return false;
    }

    // Add manifest first so it appears at the top of most unzip listings
    if (!writer.addData(manifest.toUtf8(), QStringLiteral("manifest.json"))) {
        if (error) *error = QStringLiteral("Failed to add manifest to ZIP");
        return false;
    }

    for (const QString& f : files) {
        QFileInfo fi(f);
        if (!fi.exists()) {
            qWarning() << "[DiagnosticExporter] Missing file, skipping:" << f;
            continue;
        }
        if (!writer.addFile(f, fi.fileName())) {
            if (error) *error = QStringLiteral("Failed to add %1 to ZIP").arg(fi.fileName());
            return false;
        }
    }

    if (!writer.close()) {
        if (error) *error = QStringLiteral("Failed to finalize ZIP");
        return false;
    }

    qDebug() << "[DiagnosticExporter] Bundle exported to" << outputPath;
    return true;
}

void DiagnosticExporter::exportBundleAsync(const QString& outputPath) {
    [[maybe_unused]] auto future = QtConcurrent::run([this, outputPath]() {
        QString err;
        bool ok = exportBundle(outputPath, &err);
        // Marshal result back to main thread
        QMetaObject::invokeMethod(this, [this, ok, outputPath, err]() {
            emit exportFinished(ok, outputPath, err);
        }, Qt::QueuedConnection);
    });
}

void DiagnosticExporter::onExportRequested(const QString& outputPath) {
    exportBundleAsync(outputPath);
}

// ── Private helpers ───────────────────────────────────────────────────

bool DiagnosticExporter::collectFiles(QStringList& outFiles, QString& outManifestData) {
    outFiles.clear();

    // Collect log files: active + rotated backups
    if (!m_logFilePath.isEmpty()) {
        if (QFile::exists(m_logFilePath)) {
            outFiles << m_logFilePath;
        }
        for (int i = 1; i < m_logMaxFiles; ++i) {
            QString backup = QStringLiteral("%1.%2").arg(m_logFilePath).arg(i);
            if (QFile::exists(backup)) {
                outFiles << backup;
            }
        }
    }

    // Collect config
    if (!m_configPath.isEmpty() && QFile::exists(m_configPath)) {
        outFiles << m_configPath;
    }

    QList<qint64> sizes;
    for (const QString& f : outFiles) {
        sizes << QFileInfo(f).size();
    }

    outManifestData = manifestToJson(gitRevision(), outFiles, sizes);
    return true;
}

QString DiagnosticExporter::gitRevision() {
    QProcess proc;
    proc.start(QStringLiteral("git"), QStringList{QStringLiteral("rev-parse"), QStringLiteral("HEAD")});
    if (!proc.waitForFinished(3000)) {
        return QStringLiteral("unknown");
    }
    QByteArray out = proc.readAllStandardOutput().trimmed();
    if (out.isEmpty() || proc.exitCode() != 0) {
        return QStringLiteral("unknown");
    }
    return QString::fromUtf8(out);
}

QString DiagnosticExporter::manifestToJson(const QString& gitRev,
                                            const QStringList& files,
                                            const QList<qint64>& sizes) {
    QJsonObject manifest;
    manifest.insert(QStringLiteral("manifestVersion"), 1);
    manifest.insert(QStringLiteral("application"), QStringLiteral("AEGIS GCS"));
    manifest.insert(QStringLiteral("createdAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    manifest.insert(QStringLiteral("gitRevision"), gitRev);

    QJsonArray fileArray;
    for (int i = 0; i < files.size(); ++i) {
        QJsonObject entry;
        entry.insert(QStringLiteral("path"), QFileInfo(files[i]).fileName());
        entry.insert(QStringLiteral("size"), sizes[i]);
        fileArray.append(entry);
    }
    manifest.insert(QStringLiteral("files"), fileArray);

    return QString::fromUtf8(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
}

} // namespace aegis::utils
