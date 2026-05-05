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
#include <QHostAddress>
#include <QDebug>
#include <QTimer>
#include <QtMath>

namespace {

aegis::utils::LogLevel parseLogLevel(const QString& value) {
    const QString normalized = value.trimmed().toLower();
    if (normalized == "debug") return aegis::utils::LogLevel::Debug;
    if (normalized == "info") return aegis::utils::LogLevel::Info;
    if (normalized == "warning" || normalized == "warn") return aegis::utils::LogLevel::Warning;
    if (normalized == "error") return aegis::utils::LogLevel::Error;
    if (normalized == "critical") return aegis::utils::LogLevel::Critical;
    return aegis::utils::LogLevel::Debug;
}

QStringList defaultAutostartPlugins() {
    return {
        QStringLiteral("aegis.plugins.telemetry_hud"),
        QStringLiteral("aegis.plugins.alert_console"),
        QStringLiteral("aegis.plugins.mission_editor"),
        QStringLiteral("aegis.plugins.map_view")
    };
}

QVariantMap defaultPluginConfig() {
    return {
        {QStringLiteral("aegis.plugins.telemetry_hud"), QVariantMap{}},
        {QStringLiteral("aegis.plugins.alert_console"), QVariantMap{
            {QStringLiteral("maxItems"), 200},
            {QStringLiteral("showTimestamps"), true}
        }},
        {QStringLiteral("aegis.plugins.mission_editor"), QVariantMap{}},
        {QStringLiteral("aegis.plugins.map_view"), QVariantMap{
            {QStringLiteral("zoom"), 15},
            {QStringLiteral("tileUrlTemplate"), QStringLiteral("https://tile.openstreetmap.org/%1/%2/%3.png")}
        }}
    };
}

QVariantMap mergeVariantMaps(const QVariantMap& defaults, const QVariantMap& overrides) {
    QVariantMap result = defaults;
    for (auto it = overrides.constBegin(); it != overrides.constEnd(); ++it) {
        if (result.value(it.key()).metaType().id() == QMetaType::QVariantMap &&
            it.value().metaType().id() == QMetaType::QVariantMap) {
            result.insert(it.key(), mergeVariantMaps(result.value(it.key()).toMap(), it.value().toMap()));
        } else {
            result.insert(it.key(), it.value());
        }
    }
    return result;
}

QVariantMap defaultConfig() {
    return {
        {QStringLiteral("pluginPaths"), QStringList{
            QStringLiteral("./plugins"),
            QStringLiteral("C:/Program Files/Aegis/plugins")
        }},
        {QStringLiteral("autostartPlugins"), defaultAutostartPlugins()},
        {QStringLiteral("telemetry"), QVariantMap{
            {QStringLiteral("bindAddress"), QStringLiteral("0.0.0.0")},
            {QStringLiteral("bindPort"), 14550},
            {QStringLiteral("heartbeatTimeoutMs"), 3000}
        }},
        {QStringLiteral("logging"), QVariantMap{
            {QStringLiteral("minLevel"), QStringLiteral("Debug")},
            {QStringLiteral("file"), QStringLiteral("aegis.log")}
        }},
        {QStringLiteral("dummyTelemetry"), QVariantMap{
            {QStringLiteral("enabled"), true},
            {QStringLiteral("intervalMs"), 100}
        }},
        {QStringLiteral("plugins"), defaultPluginConfig()}
    };
}

QVariantMap validateAndNormalizeConfig(const QVariantMap& rawConfig) {
    QVariantMap config = mergeVariantMaps(defaultConfig(), rawConfig);

    QVariantMap telemetry = config.value("telemetry").toMap();
    const QString bindAddress = telemetry.value("bindAddress").toString().trimmed();
    if (QHostAddress(bindAddress).isNull()) {
        qWarning() << "[Application] Invalid telemetry.bindAddress, using default";
        telemetry.insert("bindAddress", QStringLiteral("0.0.0.0"));
    }

    const int bindPort = telemetry.value("bindPort").toInt();
    telemetry.insert("bindPort", (bindPort > 0 && bindPort <= 65535) ? bindPort : 14550);

    const int heartbeatTimeoutMs = telemetry.value("heartbeatTimeoutMs").toInt();
    telemetry.insert("heartbeatTimeoutMs", heartbeatTimeoutMs > 0 ? heartbeatTimeoutMs : 3000);
    config.insert("telemetry", telemetry);

    QVariantMap logging = config.value("logging").toMap();
    logging.insert("minLevel", [&]() -> QString {
        const QString level = logging.value("minLevel").toString();
        const QString normalized = level.trimmed().toLower();
        if (normalized == "debug" || normalized == "info" || normalized == "warning" ||
            normalized == "warn" || normalized == "error" || normalized == "critical") {
            return level;
        }
        qWarning() << "[Application] Invalid logging.minLevel, using default";
        return QStringLiteral("Debug");
    }());
    config.insert("logging", logging);

    QVariantMap dummyTelemetry = config.value("dummyTelemetry").toMap();
    const int intervalMs = dummyTelemetry.value("intervalMs").toInt();
    dummyTelemetry.insert("intervalMs", intervalMs > 0 ? intervalMs : 100);
    config.insert("dummyTelemetry", dummyTelemetry);

    QVariantMap plugins = defaultPluginConfig();
    plugins = mergeVariantMaps(plugins, config.value("plugins").toMap());

    QVariantMap alertConsole = plugins.value("aegis.plugins.alert_console").toMap();
    const int maxItems = alertConsole.value("maxItems", 200).toInt();
    alertConsole.insert("maxItems", maxItems > 0 ? maxItems : 200);
    plugins.insert("aegis.plugins.alert_console", alertConsole);

    config.insert("plugins", plugins);
    return config;
}

}

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
    m_config = validateAndNormalizeConfig(m_config);

    // ── Core services ─────────────────────────────────────────────────
    m_bus.reset(new aegis::core::TelemetryBus(this));
    m_state.reset(new aegis::core::VehicleState(this));
    m_pluginHost.reset(new aegis::core::PluginHost(
        m_bus.data(), m_state.data(), m_config.value("plugins").toMap(), this));

    connect(m_pluginHost.data(), &aegis::core::PluginHost::pluginLoaded,
            this, &Application::onPluginLoaded);

    // ── Telemetry layer ───────────────────────────────────────────────
    m_mavlinkIO.reset(new aegis::telemetry::MavlinkIO());
    m_mavlinkParser.reset(new aegis::telemetry::MavlinkParser(
        m_bus.data(), m_state.data(), this));

    connect(m_mavlinkIO.data(), &aegis::telemetry::MavlinkIO::messageReceived,
            m_mavlinkParser.data(), &aegis::telemetry::MavlinkParser::processMessage,
            Qt::QueuedConnection);

    m_logReplay.reset(new aegis::telemetry::LogReplay(m_bus.data(), this));
    connect(m_logReplay.data(), &aegis::telemetry::LogReplay::messageReplayed,
            m_mavlinkParser.data(), &aegis::telemetry::MavlinkParser::processMessage,
            Qt::QueuedConnection);

    const QVariantMap telemetryConfig = m_config.value("telemetry").toMap();
    m_mavlinkIO->setHeartbeatTimeoutMs(
        telemetryConfig.value("heartbeatTimeoutMs", 3000).toInt());

    const QVariantMap loggingConfig = m_config.value("logging").toMap();
    const QString logFile = loggingConfig.value("file").toString();
    if (!logFile.isEmpty()) {
        aegis::utils::Logger::instance().setLogFile(logFile);
    }
    aegis::utils::Logger::instance().setMinLevel(
        parseLogLevel(loggingConfig.value("minLevel", "Debug").toString()));

    // ── UI layer ──────────────────────────────────────────────────────
    m_mainWindow.reset(new aegis::ui::MainWindow());
    setupConnections();

    // ── Plugin discovery ──────────────────────────────────────────────
    QStringList pluginPaths;
    pluginPaths << (QCoreApplication::applicationDirPath() + "/plugins");
    pluginPaths << (QDir::currentPath() + "/plugins");

    const QStringList configuredPluginPaths = m_config.value("pluginPaths").toStringList();
    for (const QString& path : configuredPluginPaths) {
        if (!pluginPaths.contains(path)) {
            pluginPaths << path;
        }
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

    const QVariantMap dummyTelemetryConfig = m_config.value("dummyTelemetry").toMap();
    const bool dummyTelemetryEnabled = dummyTelemetryConfig.value("enabled", false).toBool();
    if (dummyTelemetryEnabled) {
        QTimer* dummyTimer = new QTimer(this);
        connect(dummyTimer, &QTimer::timeout, this, &Application::emitDummyTelemetry);
        dummyTimer->start(dummyTelemetryConfig.value("intervalMs", 100).toInt());
        m_dummyTimer.reset(dummyTimer);

        // Tell the UI that the dummy telemetry stream is active so the map/status
        // bar look alive even before a real MAVLink UDP link is opened.
        m_bus->emitConnectionStateChanged(aegis::core::types::ConnectionState::Connected);
    }

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
            m_mainWindow.data(), &aegis::ui::MainWindow::updateConnectionState);
    connect(m_bus.data(), &aegis::core::TelemetryBus::heartbeatReceived,
            m_mainWindow.data(), &aegis::ui::MainWindow::updateSystemState);

    connect(m_mavlinkIO.data(), &aegis::telemetry::MavlinkIO::connectionStateChanged,
            this, [this](bool connected) {
        m_bus->emitConnectionStateChanged(connected
            ? aegis::core::types::ConnectionState::Connected
            : aegis::core::types::ConnectionState::Disconnected);
    });

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
    const QVariantMap telemetryConfig = m_config.value("telemetry").toMap();
    const QString bindHost = host.isEmpty()
        ? telemetryConfig.value("bindAddress", QStringLiteral("0.0.0.0")).toString()
        : host;
    const quint16 bindPort = port == 0
        ? static_cast<quint16>(telemetryConfig.value("bindPort", 14550).toUInt())
        : port;

    if (m_dummyTimer) {
        m_dummyTimer->stop();
        m_dummyTimer.reset();
    }

    m_mavlinkIO->start(QHostAddress(bindHost), bindPort);
}

void Application::onReplayRequested(const QString& filePath) {
    if (m_logReplay->loadFile(filePath)) {
        m_logReplay->start(1.0);
    }
}

void Application::onPluginLoadRequested(const QString& pluginId) {
    if (auto* plugin = m_pluginHost->load(pluginId)) {
        m_mainWindow->injectPlugin(plugin);
    }
}

void Application::onPluginLoaded(const QString& pluginId) {
    for (auto* p : m_pluginHost->activePlugins()) {
        if (p->pluginId() == pluginId) {
            m_mainWindow->injectPlugin(p);
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
