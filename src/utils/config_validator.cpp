#include "config_validator.hpp"
#include <QVariant>
#include <QDebug>

namespace aegis::utils {

void ConfigValidator::addTopLevelRule(const SchemaRule& rule) {
    m_topLevelRules.append(rule);
}

QVariantMap ConfigValidator::validate(const QVariantMap& raw, QStringList* warnings) const {
    // Build a lookup map for top-level rules
    QMap<QString, SchemaRule> schema;
    for (const auto& rule : m_topLevelRules) {
        schema.insert(rule.key, rule);
    }
    return validateObject(schema, raw, QString(), warnings, true);
}

QVariantMap ConfigValidator::validateObject(
    const QMap<QString, SchemaRule>& schema,
    const QVariantMap& raw,
    const QString& pathPrefix,
    QStringList* warnings,
    bool preserveUnknown) const {

    QVariantMap result;

    // First pass: apply schema rules (defaults + validation)
    for (auto it = schema.constBegin(); it != schema.constEnd(); ++it) {
        const QString& key = it.key();
        const SchemaRule& rule = it.value();
        const QString path = pathPrefix.isEmpty() ? key : pathPrefix + "." + key;

        if (raw.contains(key)) {
            result.insert(key, validateValue(rule, raw.value(key), path, warnings));
        } else {
            if (rule.required && warnings) {
                warnings->append(QStringLiteral("%1: missing required key; using default %2")
                    .arg(path, rule.defaultValue.toString()));
            }
            // For objects with nested schema but no explicit default, build from nested defaults
            if (rule.type == QStringLiteral("object") && !rule.nested.isEmpty() && !rule.defaultValue.isValid()) {
                result.insert(key, validateObject(rule.nested, QVariantMap{}, path, warnings, false));
            } else {
                result.insert(key, rule.defaultValue);
            }
        }
    }

    // Second pass: preserve unknown keys (forward compatibility)
    if (preserveUnknown) {
        for (auto it = raw.constBegin(); it != raw.constEnd(); ++it) {
            const QString& key = it.key();
            if (!schema.contains(key)) {
                const QString path = pathPrefix.isEmpty() ? key : pathPrefix + "." + key;
                if (warnings) {
                    warnings->append(QStringLiteral("%1: unknown config key; preserving for forward compatibility")
                        .arg(path));
                }
                result.insert(key, it.value());
            }
        }
    }

    return result;
}

QVariant ConfigValidator::validateValue(
    const SchemaRule& rule,
    const QVariant& rawValue,
    const QString& path,
    QStringList* warnings) const {

    // Special handling for "object" type with nested schema
    if (rule.type == QStringLiteral("object") && !rule.nested.isEmpty()) {
        if (rawValue.metaType().id() == QMetaType::QVariantMap) {
            return validateObject(rule.nested, rawValue.toMap(), path, warnings, true);
        }
        if (warnings) {
            warnings->append(QStringLiteral("%1: expected object, got %2; using default")
                .arg(path, rawValue.typeName()));
        }
        return rule.defaultValue;
    }

    // Basic type coercion
    QVariant coerced = coerceToType(rule.type, rawValue, path, warnings);
    if (!coerced.isValid()) {
        if (warnings) {
            warnings->append(QStringLiteral("%1: expected %2, got unrecoverable type %3; using default %4")
                .arg(path, rule.type, rawValue.typeName(), rule.defaultValue.toString()));
        }
        return rule.defaultValue;
    }

    // Range checks for int
    if (rule.type == QStringLiteral("int") && coerced.metaType().id() == QMetaType::Int) {
        int val = coerced.toInt();
        if (rule.min.isValid()) {
            int min = rule.min.toInt();
            if (val < min) {
                if (warnings) {
                    warnings->append(QStringLiteral("%1: value %2 below minimum %3; clamped")
                        .arg(path).arg(val).arg(min));
                }
                val = min;
            }
        }
        if (rule.max.isValid()) {
            int max = rule.max.toInt();
            if (val > max) {
                if (warnings) {
                    warnings->append(QStringLiteral("%1: value %2 above maximum %3; clamped")
                        .arg(path).arg(val).arg(max));
                }
                val = max;
            }
        }
        coerced = val;
    }

    // Range checks for double
    if (rule.type == QStringLiteral("double") && coerced.metaType().id() == QMetaType::Double) {
        double val = coerced.toDouble();
        if (rule.min.isValid()) {
            double min = rule.min.toDouble();
            if (val < min) {
                if (warnings) {
                    warnings->append(QStringLiteral("%1: value %2 below minimum %3; clamped")
                        .arg(path).arg(val).arg(min));
                }
                val = min;
            }
        }
        if (rule.max.isValid()) {
            double max = rule.max.toDouble();
            if (val > max) {
                if (warnings) {
                    warnings->append(QStringLiteral("%1: value %2 above maximum %3; clamped")
                        .arg(path).arg(val).arg(max));
                }
                val = max;
            }
        }
        coerced = val;
    }

    // Allowed values check for string
    if (rule.type == QStringLiteral("string") && !rule.allowed.isEmpty()) {
        QString val = coerced.toString();
        QString normalized = val.trimmed().toLower();
        bool found = false;
        for (const QString& a : rule.allowed) {
            if (normalized == a.toLower()) {
                found = true;
                break;
            }
        }
        if (!found) {
            if (warnings) {
                warnings->append(QStringLiteral("%1: value '%2' not in allowed set [%3]; using default '%4'")
                    .arg(path, val, rule.allowed.join(", "), rule.defaultValue.toString()));
            }
            coerced = rule.defaultValue;
        }
    }

    return coerced;
}

QVariant ConfigValidator::coerceToType(
    const QString& type,
    const QVariant& value,
    [[maybe_unused]] const QString& path,
    [[maybe_unused]] QStringList* warnings) const {

    if (type == QStringLiteral("string")) {
        if (value.canConvert<QString>()) {
            return value.toString();
        }
        return QVariant();
    }

    if (type == QStringLiteral("int")) {
        if (value.metaType().id() == QMetaType::Int ||
            value.metaType().id() == QMetaType::LongLong ||
            value.metaType().id() == QMetaType::UInt ||
            value.metaType().id() == QMetaType::ULongLong) {
            return value.toInt();
        }
        if (value.metaType().id() == QMetaType::Double) {
            double d = value.toDouble();
            if (d == static_cast<int>(d)) {
                return static_cast<int>(d);
            }
        }
        if (value.metaType().id() == QMetaType::QString) {
            bool ok = false;
            int i = value.toString().toInt(&ok);
            if (ok) return i;
        }
        return QVariant();
    }

    if (type == QStringLiteral("double")) {
        if (value.canConvert<double>()) {
            return value.toDouble();
        }
        if (value.metaType().id() == QMetaType::QString) {
            bool ok = false;
            double d = value.toString().toDouble(&ok);
            if (ok) return d;
        }
        return QVariant();
    }

    if (type == QStringLiteral("bool")) {
        if (value.metaType().id() == QMetaType::Bool) {
            return value.toBool();
        }
        if (value.metaType().id() == QMetaType::Int) {
            int i = value.toInt();
            return i != 0;
        }
        if (value.metaType().id() == QMetaType::QString) {
            QString s = value.toString().trimmed().toLower();
            if (s == "true" || s == "1" || s == "yes" || s == "on") return true;
            if (s == "false" || s == "0" || s == "no" || s == "off") return false;
        }
        return QVariant();
    }

    if (type == QStringLiteral("array")) {
        if (value.canConvert<QVariantList>()) {
            return value;
        }
        return QVariant();
    }

    if (type == QStringLiteral("object")) {
        if (value.metaType().id() == QMetaType::QVariantMap) {
            return value;
        }
        return QVariant();
    }

    // Unknown type: pass through
    return value;
}

// ── AEGIS schema factory ───────────────────────────────────────────

ConfigValidator ConfigValidator::createAegisValidator() {
    ConfigValidator v;

    v.addTopLevelRule(SchemaRule{
        QStringLiteral("configVersion"),
        QStringLiteral("int"),
        false, QVariant(1),
        QVariant(1), QVariant()  // no max
    });

    v.addTopLevelRule(SchemaRule{
        QStringLiteral("pluginPaths"),
        QStringLiteral("array"),
        false,
        QVariant(QStringList{
            QStringLiteral("./plugins"),
            QStringLiteral("C:/Program Files/Aegis/plugins")
        })
    });

    v.addTopLevelRule(SchemaRule{
        QStringLiteral("autostartPlugins"),
        QStringLiteral("array"),
        false,
        QVariant(QStringList{
            QStringLiteral("aegis.plugins.telemetry_hud"),
            QStringLiteral("aegis.plugins.alert_console"),
            QStringLiteral("aegis.plugins.mission_editor"),
            QStringLiteral("aegis.plugins.map_view")
        })
    });

    QMap<QString, SchemaRule> telemetryNested;
    telemetryNested.insert(QStringLiteral("bindAddress"), SchemaRule{
        QStringLiteral("bindAddress"), QStringLiteral("string"), false,
        QVariant(QStringLiteral("0.0.0.0"))});
    telemetryNested.insert(QStringLiteral("bindPort"), SchemaRule{
        QStringLiteral("bindPort"), QStringLiteral("int"), false,
        QVariant(14550), QVariant(1), QVariant(65535)});
    telemetryNested.insert(QStringLiteral("heartbeatTimeoutMs"), SchemaRule{
        QStringLiteral("heartbeatTimeoutMs"), QStringLiteral("int"), false,
        QVariant(3000), QVariant(100)});
    v.addTopLevelRule(SchemaRule{
        QStringLiteral("telemetry"), QStringLiteral("object"), false,
        QVariant(), QVariant(), QVariant(), QStringList(), telemetryNested});

    QMap<QString, SchemaRule> loggingNested;
    loggingNested.insert(QStringLiteral("minLevel"), SchemaRule{
        QStringLiteral("minLevel"), QStringLiteral("string"), false,
        QVariant(QStringLiteral("Debug")),
        QVariant(), QVariant(),
        QStringList{
            QStringLiteral("Debug"), QStringLiteral("Info"),
            QStringLiteral("Warning"), QStringLiteral("Warn"),
            QStringLiteral("Error"), QStringLiteral("Critical")
        }});
    loggingNested.insert(QStringLiteral("file"), SchemaRule{
        QStringLiteral("file"), QStringLiteral("string"), false,
        QVariant(QStringLiteral("aegis.log"))});
    loggingNested.insert(QStringLiteral("maxFiles"), SchemaRule{
        QStringLiteral("maxFiles"), QStringLiteral("int"), false,
        QVariant(5), QVariant(1)});
    loggingNested.insert(QStringLiteral("maxSizeBytes"), SchemaRule{
        QStringLiteral("maxSizeBytes"), QStringLiteral("int"), false,
        QVariant(10485760), QVariant(1024)});
    v.addTopLevelRule(SchemaRule{
        QStringLiteral("logging"), QStringLiteral("object"), false,
        QVariant(), QVariant(), QVariant(), QStringList(), loggingNested});

    QMap<QString, SchemaRule> dummyTelemetryNested;
    dummyTelemetryNested.insert(QStringLiteral("enabled"), SchemaRule{
        QStringLiteral("enabled"), QStringLiteral("bool"), false,
        QVariant(true)});
    dummyTelemetryNested.insert(QStringLiteral("intervalMs"), SchemaRule{
        QStringLiteral("intervalMs"), QStringLiteral("int"), false,
        QVariant(100), QVariant(10)});
    v.addTopLevelRule(SchemaRule{
        QStringLiteral("dummyTelemetry"), QStringLiteral("object"), false,
        QVariant(), QVariant(), QVariant(), QStringList(), dummyTelemetryNested});

    QMap<QString, SchemaRule> diagnosticsNested;
    diagnosticsNested.insert(QStringLiteral("exportPath"), SchemaRule{
        QStringLiteral("exportPath"), QStringLiteral("string"), false,
        QVariant(QStringLiteral("aegis-diagnostics.zip"))});
    v.addTopLevelRule(SchemaRule{
        QStringLiteral("diagnostics"), QStringLiteral("object"), false,
        QVariant(), QVariant(), QVariant(), QStringList(), diagnosticsNested});

    QMap<QString, SchemaRule> pluginsNested;
    pluginsNested.insert(QStringLiteral("aegis.plugins.telemetry_hud"), SchemaRule{
        QStringLiteral("aegis.plugins.telemetry_hud"), QStringLiteral("object"), false,
        QVariant()});

    QMap<QString, SchemaRule> alertConsoleNested;
    alertConsoleNested.insert(QStringLiteral("maxItems"), SchemaRule{
        QStringLiteral("maxItems"), QStringLiteral("int"), false,
        QVariant(200), QVariant(1)});
    alertConsoleNested.insert(QStringLiteral("showTimestamps"), SchemaRule{
        QStringLiteral("showTimestamps"), QStringLiteral("bool"), false,
        QVariant(true)});
    pluginsNested.insert(QStringLiteral("aegis.plugins.alert_console"), SchemaRule{
        QStringLiteral("aegis.plugins.alert_console"), QStringLiteral("object"), false,
        QVariant(), QVariant(), QVariant(), QStringList(), alertConsoleNested});

    pluginsNested.insert(QStringLiteral("aegis.plugins.mission_editor"), SchemaRule{
        QStringLiteral("aegis.plugins.mission_editor"), QStringLiteral("object"), false,
        QVariant()});

    QMap<QString, SchemaRule> mapViewNested;
    mapViewNested.insert(QStringLiteral("zoom"), SchemaRule{
        QStringLiteral("zoom"), QStringLiteral("int"), false,
        QVariant(15), QVariant(1), QVariant(20)});
    mapViewNested.insert(QStringLiteral("tileUrlTemplate"), SchemaRule{
        QStringLiteral("tileUrlTemplate"), QStringLiteral("string"), false,
        QVariant(QStringLiteral("https://tile.openstreetmap.org/%1/%2/%3.png"))});
    pluginsNested.insert(QStringLiteral("aegis.plugins.map_view"), SchemaRule{
        QStringLiteral("aegis.plugins.map_view"), QStringLiteral("object"), false,
        QVariant(), QVariant(), QVariant(), QStringList(), mapViewNested});

    v.addTopLevelRule(SchemaRule{
        QStringLiteral("plugins"), QStringLiteral("object"), false,
        QVariant(), QVariant(), QVariant(), QStringList(), pluginsNested});

    return v;
}

} // namespace aegis::utils
