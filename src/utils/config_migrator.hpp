#pragma once

#include <QJsonObject>
#include <QMap>
#include <functional>
#include <QString>

namespace aegis::utils {

/**
 * @brief Registry and runner for versioned config migrations.
 *
 * Migrations are pure data transforms: QJsonObject in → QJsonObject out.
 * The migrator chains multiple steps (v1→v2→v3) automatically.
 * Before migration, a timestamped backup is written.
 */
class ConfigMigrator {
public:
    using MigrationFunc = std::function<QJsonObject(const QJsonObject&)>;

    static constexpr int CurrentVersion = 2;

    ConfigMigrator() = default;

    /** @brief Register a transform from <code>fromVersion</code> to <code>fromVersion + 1</code>. */
    void registerMigration(int fromVersion, MigrationFunc func);

    /**
     * @brief Migrate a config up to <code>targetVersion</code>.
     * @param config The original config JSON object.
     * @param targetVersion The version to migrate to.
     * @param error Optional string to populate if migration fails.
     * @return The migrated config object, or the original on failure.
     */
    QJsonObject migrate(const QJsonObject& config, int targetVersion,
                        QString* error = nullptr) const;

    /**
     * @brief Write a timestamped backup of the original config.
     * @param original The original JSON object.
     * @param basePath The path to the original config file (used for backup naming).
     * @param backupPath Optional output: the path where backup was written.
     * @return true if backup was written successfully.
     */
    static bool writeBackup(const QJsonObject& original,
                            const QString& basePath,
                            QString* backupPath = nullptr);

    /**
     * @brief Factory: builds a migrator with all known AEGIS migrations.
     */
    static ConfigMigrator createAegisMigrator();

private:
    QMap<int, MigrationFunc> m_registry; ///< fromVersion → transform
};

} // namespace aegis::utils
