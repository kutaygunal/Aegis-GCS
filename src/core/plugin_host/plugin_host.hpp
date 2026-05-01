#pragma once

#include <QObject>
#include <QDir>
#include <QHash>
#include <QPluginLoader>
#include <QScopedPointer>
#include <memory>

#include "plugin_meta.hpp"
#include "../interfaces/iplugin.hpp"

namespace aegis::core {

class TelemetryBus;
class VehicleState;

/**
 * @brief Runtime plugin discovery, loading, and lifecycle management.
 *
 * Scans plugin directories for shared libraries implementing IPlugin,
 * instantiates them, and tracks active instances.
 *
 * Error isolation: a crashing plugin is caught and unloaded without
 * bringing down the entire shell.
 */
class PluginHost : public QObject {
    Q_OBJECT

public:
    explicit PluginHost(TelemetryBus* bus, VehicleState* state,
                        const QVariantMap& config,
                        QObject* parent = nullptr);
    ~PluginHost() override;

    /** @brief Scan directories for plugin libraries. */
    QList<PluginMeta> discover(const QStringList& searchPaths);

    /** @brief Load and initialize a plugin by ID. */
    IPlugin* load(const QString& pluginId);

    /** @brief Gracefully shut down and unload a plugin. */
    void unload(const QString& pluginId);

    /** @brief Tear down every active plugin. */
    void unloadAll();

    /** @return List of currently loaded plugin instances. */
    QList<IPlugin*> activePlugins() const;

    /** @return Metadata for all discovered plugins. */
    QList<PluginMeta> registry() const { return m_registry.values(); }

    /** @return true if the plugin is currently loaded. */
    bool isLoaded(const QString& pluginId) const;

signals:
    void pluginLoaded(const QString& pluginId);
    void pluginUnloaded(const QString& pluginId);
    void pluginCrashed(const QString& pluginId, const QString& reason);

private:
    TelemetryBus* m_bus;
    VehicleState* m_state;
    QVariantMap m_config;

    // pluginId -> metadata
    QHash<QString, PluginMeta> m_registry;

    // pluginId -> {loader, instance}
    struct LoadedPlugin {
        QScopedPointer<QPluginLoader> loader;
        IPlugin* instance{nullptr};
    };
    QHash<QString, LoadedPlugin> m_active;
};

} // namespace aegis::core
