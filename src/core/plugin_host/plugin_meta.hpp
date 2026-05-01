#pragma once

#include <QString>
#include <QVersionNumber>

namespace aegis::core {

/**
 * @brief Lightweight metadata container for discovered plugins.
 */
struct PluginMeta {
    QString pluginId;
    QString name;
    QString version;
    QString author;
    QString category;
    QString libraryPath;
    QStringList permissions;

    QString displayName() const {
        return QString("%1 v%2").arg(name, version);
    }
};

} // namespace aegis::core

Q_DECLARE_METATYPE(aegis::core::PluginMeta)
