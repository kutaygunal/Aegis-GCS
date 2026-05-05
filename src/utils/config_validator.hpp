#pragma once

#include <QObject>
#include <QVariantMap>
#include <QMap>
#include <QStringList>

namespace aegis::utils {

/**
 * @brief Describes a single config key validation rule.
 */
struct SchemaRule {
    QString key;              ///< Key name (e.g., "bindPort")
    QString type;             ///< "string", "int", "bool", "double", "array", "object"
    bool required = false;    ///< If true, missing key emits a warning
    QVariant defaultValue;    ///< Value used when key is missing or invalid
    QVariant min;             ///< Minimum for int/double (inclusive)
    QVariant max;             ///< Maximum for int/double (inclusive)
    QStringList allowed;      ///< Allowed string values (empty = any string)
    QMap<QString, SchemaRule> nested; ///< Child rules for "object" type
};

/**
 * @brief Lightweight declarative config validator.
 *
 * Validates a raw QVariantMap (from JSON parse) against a declared schema,
 * produces warnings with key-path context, and returns a normalized map
 * with defaults filled in, ranges clamped, and types coerced.
 *
 * Unknown keys are preserved (forward compatibility) but warned.
 */
class ConfigValidator {
public:
    ConfigValidator() = default;

    /** @brief Add a validation rule for a top-level key. */
    void addTopLevelRule(const SchemaRule& rule);

    /**
     * @brief Validate and normalize a raw config map.
     * @param raw The unvalidated config map from JSON parse.
     * @param warnings Optional list to populate with human-readable warnings.
     * @return Normalized config map with defaults, clamped ranges, and coerced types.
     */
    QVariantMap validate(const QVariantMap& raw, QStringList* warnings = nullptr) const;

    /**
     * @brief Convenience factory: builds a validator with the full AEGIS schema.
     */
    static ConfigValidator createAegisValidator();

private:
    QVariantMap validateObject(
        const QMap<QString, SchemaRule>& schema,
        const QVariantMap& raw,
        const QString& pathPrefix,
        QStringList* warnings,
        bool preserveUnknown) const;

    QVariant validateValue(
        const SchemaRule& rule,
        const QVariant& rawValue,
        const QString& path,
        QStringList* warnings) const;

    QVariant coerceToType(
        const QString& type,
        const QVariant& value,
        const QString& path,
        QStringList* warnings) const;

    QList<SchemaRule> m_topLevelRules;
};

} // namespace aegis::utils
