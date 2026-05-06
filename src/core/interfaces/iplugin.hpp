#pragma once

#include <QObject>
#include <QtPlugin>
#include <QWidget>
#include <QString>
#include <QVersionNumber>

namespace aegis::core {

class TelemetryBus;
class VehicleState;

/**
 * @brief Base contract for all AEGIS plugins.
 *
 * Plugins are compiled as shared libraries and loaded at runtime via QPluginLoader.
 * Each plugin receives a reference to the central TelemetryBus and VehicleState model.
 */
class IPlugin : public QObject {
    Q_OBJECT

public:
    virtual ~IPlugin() = default;

    /**
     * @brief Called once when the plugin is loaded into the shell.
     * @param bus   The global telemetry pub/sub bus.
     * @param state The canonical vehicle state model.
     * @param config Key/value configuration from config files (*.json).
     * @return true on success; on false, the plugin will be unloaded.
     */
    virtual bool initialize(TelemetryBus* bus, VehicleState* state,
                            const QVariantMap& config) = 0;

    /** @return Primary Qt widget displayed in the dockable workspace. */
    virtual QWidget* widget() = 0;

    /** @brief Release resources, unsubscribe, and clean up before unload. */
    virtual void shutdown() = 0;

    /** @brief Human-readable plugin metadata. */
    virtual QString name() const = 0;
    virtual QString pluginId() const = 0;
    virtual QString version() const = 0;
    virtual QString author() const = 0;
    virtual QString category() const = 0;

    /** @brief List of required permissions (e.g. "command:nav", "read:mission"). */
    virtual QStringList permissions() const { return {}; }

protected:
    explicit IPlugin(QObject* parent = nullptr) : QObject(parent) {}
};

} // namespace aegis::core

#define AegisPluginInterface_iid "com.aegis.gcs.IPlugin/1.0"
Q_DECLARE_INTERFACE(aegis::core::IPlugin, AegisPluginInterface_iid)
