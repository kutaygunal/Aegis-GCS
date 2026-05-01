#include "application.hpp"
#include "core/bus/telemetry_bus.hpp"
#include "core/state/vehicle_state.hpp"
#include "core/plugin_host/plugin_host.hpp"
#include "telemetry/mavlink_io.hpp"
#include "telemetry/parsers.hpp"
#include "telemetry/replay/log_replay.hpp"
#include "ui/main_window.hpp"
#include "utils/logging.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>

namespace aegis::app {

Application::Application(QObject* parent) : QObject(parent) {}

Application::~Application() {
    if (m_pluginHost) {
        m_pluginHost->unloadAll();
    }
    if (m_mavlinkIO) {
        m_mavlinkIO->stop();
    }
}

bool Application::initialize() {
    if (!loadConfiguration()) {
        qWarning() << "[Application] Using default configuration";
    }

    // ── Core services ─────────────────────────────────────────────────
    m_bus.reset(new aegis::core::TelemetryBus(this));
    m_state.reset(new aegis::core::VehicleState(this));
    m_pluginHost.reset(new aegis::core::PluginHost(
        m_bus.data(), m_state.data(), m_config.value("plugins").toMap(), this));

    // ── Telemetry layer ───────────────────────────────────────────────
    m_mavlinkIO.reset(new aegis::telemetry::MavlinkIO(this));
    m_mavlinkParser.reset(new aegis::telemetry::MavlinkParser(
        m_bus.data(), m_state.data(), this));

    connect(m_mavlinkIO.data(), &aegis::telemetry::MavlinkIO::messageReceived,
            m_mavlinkParser.data(), &aegis::telemetry::MavlinkParser::processMessage,
            Qt::QueuedConnection);

    m_logReplay.reset(new aegis::telemetry::LogReplay(m_bus.data(), this));

    // ── UI layer ──────────────────────────────────────────────────────
    m_mainWindow.reset(new aegis::ui::MainWindow());
    setupConnections();

    // ── Plugin discovery ──────────────────────────────────────────────
    QStringList pluginPaths = m_config.value("pluginPaths").toStringList();
    if (pluginPaths.isEmpty()) {
        pluginPaths << QDir::currentPath() + "/plugins";
    }
    auto discovered = m_pluginHost->discover(pluginPaths);
    qDebug() << "[Application] Discovered" << discovered.size() << "plugins";

    // Auto-load plugins marked as "autostart"
    for (const auto& meta : discovered) {
        if (m_config.value("autostartPlugins").toStringList().contains(meta.pluginId)) {
            m_pluginHost->load(meta.pluginId);
        }
    }

    aegis::utils::Logger::instance().setMinLevel(aegis::utils::LogLevel::Debug);
    aegis::utils::Logger::instance().log(
        aegis::utils::LogLevel::Info, "App", "AEGIS GCS initialized");

    m_initialized = true;
    return true;
}

void Application::showWindow() {
    if (m_mainWindow) {
        m_mainWindow->show();
    }
}

aegis::core::TelemetryBus* Application::bus() const {
    return m_bus.data();
}

bool Application::loadConfiguration() {
    QString configPath = QStandardPaths::locate(
        QStandardPaths::AppConfigLocation, "aegis.json");
    if (configPath.isEmpty()) {
        configPath = "config/aegis.json";
    }
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return false;

    m_config = doc.object().toVariantMap();
    return true;
}

void Application::setupConnections() {
    connect(m_mainWindow.data(), &aegis::ui::MainWindow::connectionRequested,
            this, &Application::onConnectionRequested);
    connect(m_mainWindow.data(), &aegis::ui::MainWindow::replayRequested,
            this, &Application::onReplayRequested);
    connect(m_mainWindow.data(), &aegis::ui::MainWindow::aboutToShutdown,
            this, &Application::onShutdown);

    connect(m_bus.data(), &aegis::core::TelemetryBus::connectionStateChanged,
            this, [](bool connected) {
        QString msg = connected ? "Connected" : "Disconnected";
        aegis::utils::Logger::instance().log(
            aegis::utils::LogLevel::Info, "Link", msg);
    });
}

void Application::onConnectionRequested(const QString& host, quint16 port) {
    Q_UNUSED(host)
    m_mavlinkIO->start();
    Q_UNUSED(port)
}

void Application::onReplayRequested(const QString& filePath) {
    if (m_logReplay->loadFile(filePath)) {
        m_logReplay->start(1.0);
    }
}

void Application::onPluginLoadRequested(const QString& pluginId) {
    if (auto* plugin = m_pluginHost->load(pluginId)) {
        m_mainWindow->dockManager()->injectPlugin(plugin);
    }
}

void Application::onShutdown() {
    m_pluginHost->unloadAll();
    m_mavlinkIO->stop();
    aegis::utils::Logger::instance().log(
        aegis::utils::LogLevel::Info, "App", "AEGIS GCS shutting down");
}

} // namespace aegis::app
