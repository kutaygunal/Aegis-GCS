#include "application.hpp"
#include "core/bus/telemetry_bus.hpp"
#include "core/state/vehicle_state.hpp"
#include "core/plugin_host/plugin_host.hpp"
#include "core/interfaces/iplugin.hpp"
#include "telemetry/mavlink_io.hpp"
#include "telemetry/parsers.hpp"
#include "telemetry/replay/log_replay.hpp"
#include "ui/main_window.hpp"
#include "ui/dock_manager.hpp"
#include "utils/logging.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <QTimer>
#include <QtMath>

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
        // Set fallback autostart plugins if no config file
        m_config.insert("autostartPlugins", QStringList()
            << "aegis.plugins.telemetry_hud"
            << "aegis.plugins.alert_console"
            << "aegis.plugins.mission_editor"
            << "aegis.plugins.map_view");
    }

    // ── Core services ─────────────────────────────────────────────────
    m_bus.reset(new aegis::core::TelemetryBus(this));
    m_state.reset(new aegis::core::VehicleState(this));
    m_pluginHost.reset(new aegis::core::PluginHost(
        m_bus.data(), m_state.data(), m_config.value("plugins").toMap(), this));

    connect(m_pluginHost.data(), &aegis::core::PluginHost::pluginLoaded,
            this, &Application::onPluginLoaded);

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
        pluginPaths << QCoreApplication::applicationDirPath() + "/plugins";
        pluginPaths << QDir::currentPath() + "/plugins";  // fallback
    }
    auto discovered = m_pluginHost->discover(pluginPaths);
    qDebug() << "[Application] Discovered" << discovered.size() << "plugins";

    // Auto-load plugins marked as "autostart"
    QStringList autostart = m_config.value("autostartPlugins").toStringList();
    for (const auto& meta : discovered) {
        if (autostart.contains(meta.pluginId)) {
            m_pluginHost->load(meta.pluginId);
        }
    }

    // ── Dummy telemetry generator (for demo when no vehicle is connected) ─
    QTimer* dummyTimer = new QTimer(this);
    connect(dummyTimer, &QTimer::timeout, this, &Application::emitDummyTelemetry);
    dummyTimer->start(100);  // 10 Hz
    m_dummyTimer.reset(dummyTimer);

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
            this, [](aegis::core::types::ConnectionState state) {
        QString msg = (state == aegis::core::types::ConnectionState::Connected)
                          ? "Connected" : "Disconnected";
        aegis::utils::Logger::instance().log(
            aegis::utils::LogLevel::Info, "Link", msg);
    });

    connect(m_bus.data(), &aegis::core::TelemetryBus::outboundCommand,
            m_mavlinkIO.data(), &aegis::telemetry::MavlinkIO::sendCommand,
            Qt::QueuedConnection);
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

void Application::onPluginLoaded(const QString& pluginId) {
    for (auto* p : m_pluginHost->activePlugins()) {
        if (p->pluginId() == pluginId) {
            m_mainWindow->dockManager()->injectPlugin(p);
            qDebug() << "[Application] Injected" << pluginId;
            break;
        }
    }
}

void Application::emitDummyTelemetry() {
    static qreal t = 0.0;
    t += 0.1;

    aegis::core::types::AttitudeData attitude;
    attitude.roll  = qSin(t) * 0.5;
    attitude.pitch = qCos(t * 0.7) * 0.3;
    attitude.yaw   = t;
    attitude.timestamp = QDateTime::currentDateTimeUtc();

    aegis::core::types::PositionData position;
    position.lat = 377000000 + static_cast<qint32>(qSin(t * 0.1) * 500000);
    position.lon = -1220000000 + static_cast<qint32>(qCos(t * 0.1) * 500000);
    position.relativeAlt = static_cast<qint32>(qSin(t * 0.3) * 10000);
    position.timestamp = QDateTime::currentDateTimeUtc();

    aegis::core::types::BatteryData battery;
    battery.remaining = 80 + static_cast<qint8>(qSin(t * 0.05) * 15);
    battery.voltageV = 14.8 + qSin(t * 0.2) * 0.5;

    aegis::core::types::SystemState state;
    state.status = aegis::core::types::SystemStatus::Active;
    state.armed = true;

    m_state->updateAttitude(attitude);
    m_state->updatePosition(position);
    m_state->updateBattery(battery);
    m_state->updateSystemState(state);

    m_bus->emitAttitudeChanged(attitude);
    m_bus->emitPositionChanged(position);
    m_bus->emitBatteryChanged(battery);
    m_bus->emitHeartbeat(state);
}

void Application::onShutdown() {
    m_pluginHost->unloadAll();
    m_mavlinkIO->stop();
    aegis::utils::Logger::instance().log(
        aegis::utils::LogLevel::Info, "App", "AEGIS GCS shutting down");
}

} // namespace aegis::app
