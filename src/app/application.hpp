#pragma once

#include <QObject>
#include <QScopedPointer>
#include <QVariantMap>

// Forward declarations
namespace aegis::core {
    class TelemetryBus;
    class VehicleState;
    class PluginHost;
}
namespace aegis::telemetry {
    class MavlinkIO;
    class MavlinkParser;
    class LogReplay;
}
namespace aegis::ui {
    class MainWindow;
}

namespace aegis::app {

/**
 * @brief Application bootstrapper and runtime coordinator.
 *
 * Owns the core service objects (Bus, State, PluginHost) and wires them
 * to the UI and telemetry layers. Handles clean shutdown ordering.
 */
class Application : public QObject {
    Q_OBJECT

public:
    explicit Application(QObject* parent = nullptr);
    ~Application() override;

    /** @brief Initialize core services and load plugins. */
    bool initialize();

    /** @brief Show the primary operator window. */
    void showWindow();

    /** @return Reference to the telemetry bus for external consumers. */
    aegis::core::TelemetryBus* bus() const;

private slots:
    void onConnectionRequested(const QString& host, quint16 port);
    void onReplayRequested(const QString& filePath);
    void onPluginLoadRequested(const QString& pluginId);
    void onShutdown();

private:
    bool loadConfiguration();
    void setupConnections();

    QScopedPointer<aegis::core::TelemetryBus> m_bus;
    QScopedPointer<aegis::core::VehicleState> m_state;
    QScopedPointer<aegis::core::PluginHost> m_pluginHost;
    QScopedPointer<aegis::telemetry::MavlinkIO> m_mavlinkIO;
    QScopedPointer<aegis::telemetry::MavlinkParser> m_mavlinkParser;
    QScopedPointer<aegis::telemetry::LogReplay> m_logReplay;
    QScopedPointer<aegis::ui::MainWindow> m_mainWindow;

    QVariantMap m_config;
    bool m_initialized{false};
};

} // namespace aegis::app
