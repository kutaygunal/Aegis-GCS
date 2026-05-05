#include "config_migrator.hpp"
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>
#include <QDebug>

namespace aegis::utils {

void ConfigMigrator::registerMigration(int fromVersion, MigrationFunc func) {
    m_registry.insert(fromVersion, func);
}

QJsonObject ConfigMigrator::migrate(const QJsonObject& config, int targetVersion,
                                     QString* error) const {
    int currentVersion = config.value("configVersion").toInt(1);
    if (currentVersion >= targetVersion) {
        return config; // Already up to date
    }

    QJsonObject result = config;
    for (int v = currentVersion; v < targetVersion; ++v) {
        auto it = m_registry.find(v);
        if (it == m_registry.end()) {
            if (error) {
                *error = QStringLiteral("Missing migration step from version %1 to %2")
                    .arg(v).arg(v + 1);
            }
            return config; // Return original on failure
        }
        result = it.value()(result);
        if (result.isEmpty()) {
            if (error) {
                *error = QStringLiteral("Migration step %1 → %2 produced invalid result")
                    .arg(v).arg(v + 1);
            }
            return config;
        }
    }
    return result;
}

bool ConfigMigrator::writeBackup(const QJsonObject& original,
                                  const QString& basePath,
                                  QString* backupPath) {
    QFileInfo fi(basePath);
    QString dir = fi.absolutePath();
    QString baseName = fi.completeBaseName();
    QString suffix = fi.suffix();
    QString timestamp = QDateTime::currentDateTimeUtc().toString("yyyyMMddThhmmss");
    QString backupName = QStringLiteral("%1.backup.%2.%3")
        .arg(baseName, timestamp, suffix);
    QString fullPath = QDir(dir).filePath(backupName);

    QJsonDocument doc(original);
    QFile file(fullPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "[ConfigMigrator] Failed to write backup to" << fullPath;
        return false;
    }
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    if (backupPath) *backupPath = fullPath;
    return true;
}

// ── AEGIS migrations ─────────────────────────────────────────────────

/**
 * @brief v1 → v2 migration
 *
 * Adds:
 *   telemetry.reconnectIntervalMs = 5000
 *   logging.enableConsole = true
 * Bumps configVersion to 2.
 * Preserves all other keys.
 */
static QJsonObject migrateV1ToV2(const QJsonObject& config) {
    QJsonObject result = config;

    QJsonObject telemetry = result.value("telemetry").toObject();
    if (!telemetry.contains("reconnectIntervalMs")) {
        telemetry.insert("reconnectIntervalMs", 5000);
    }
    result.insert("telemetry", telemetry);

    QJsonObject logging = result.value("logging").toObject();
    if (!logging.contains("enableConsole")) {
        logging.insert("enableConsole", true);
    }
    result.insert("logging", logging);

    result.insert("configVersion", 2);
    return result;
}

/**
 * @brief v2 → v3 migration (placeholder for future use)
 *
 * Currently a no-op identity transform. Registered so multi-step
 * tests can exercise the chain without needing real v3 schema changes.
 */
static QJsonObject migrateV2ToV3(const QJsonObject& config) {
    QJsonObject result = config;
    result.insert("configVersion", 3);
    return result;
}

ConfigMigrator ConfigMigrator::createAegisMigrator() {
    ConfigMigrator m;
    m.registerMigration(1, migrateV1ToV2);
    m.registerMigration(2, migrateV2ToV3);
    return m;
}

} // namespace aegis::utils
