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
#include "utils/diagnostic_exporter.hpp"
#include "utils/config_validator.hpp"
#include "utils/config_migrator.hpp"
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

} // namespace

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
    QString configFilePath = QStandardPaths::locate(
        QStandardPaths::AppConfigLocation, "aegis.json");
    if (configFilePath.isEmpty()) {
        configFilePath = "config/aegis.json";
    }

    if (!loadConfiguration()) {
        qWarning() << "[Application] Using default configuration";
    }

    // ── Migrate config if older than current version ────────────────────
    QJsonObject configJson = QJsonObject::fromVariantMap(m_config);
    int configVersion = configJson.value("configVersion").toInt(1);
    if (configVersion < aegis::utils::ConfigMigrator::CurrentVersion) {
        QString backupPath;
        if (aegis::utils::ConfigMigrator::writeBackup(configJson, configFilePath, &backupPath)) {
            aegis::utils::Logger::instance().log(
                aegis::utils::LogLevel::Info, "Config",
                QStringLiteral("Backup written to %1").arg(backupPath));
        } else {
            aegis::utils::Logger::instance().log(
                aegis::utils::LogLevel::Warning, "Config",
                QStringLiteral("Failed to write config backup before migration"));
        }

        QString error;
        aegis::utils::ConfigMigrator migrator = aegis::utils::ConfigMigrator::createAegisMigrator();
        QJsonObject migrated = migrator.migrate(configJson,
            aegis::utils::ConfigMigrator::CurrentVersion, &error);
        if (!error.isEmpty()) {
            aegis::utils::Logger::instance().log(
                aegis::utils::LogLevel::Error, "Config",
                QStringLiteral("Migration failed: %1").arg(error));
        } else {
            aegis::utils::Logger::instance().log(
                aegis::utils::LogLevel::Info, "Config",
                QStringLiteral("Migrated config from v%1 to v%2")
                    .arg(configVersion)
                    .arg(aegis::utils::ConfigMigrator::CurrentVersion));
            configJson = migrated;
        }
        m_config = configJson.toVariantMap();
    }

    // Validate config before any service initialization
    QStringList warnings;
    aegis::utils::ConfigValidator validator = aegis::utils::ConfigValidator::createAegisValidator();
    m_config = validator.validate(m_config, &warnings);
    for (const QString& w : warnings) {
        aegis::utils::Logger::instance().log(
            aegis::utils::LogLevel::Warning, "Config", w);
    }

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
    aegis::utils::Logger::instance().setRotation(
        loggingConfig.value("maxSizeBytes", 10485760).toLongLong(),
        loggingConfig.value("maxFiles", 5).toInt());

    // ── Diagnostic exporter ───────────────────────────────────────────
    const QVariantMap diagnosticsConfig = m_config.value("diagnostics").toMap();
    QString exportPath = diagnosticsConfig.value("exportPath").toString();
    if (exportPath.isEmpty()) {
        exportPath = QDir::currentPath() + "/aegis-diagnostics.zip";
    }
    m_diagnosticExporter.reset(new aegis::utils::DiagnosticExporter(
        logFile,
        loggingConfig.value("maxFiles", 5).toInt(),
        "config/aegis.json",
        this));
    connect(m_bus.data(), &aegis::core::TelemetryBus::diagnosticExportRequested,
            m_diagnosticExporter.data(), &aegis::utils::DiagnosticExporter::onExportRequested);
    connect(m_diagnosticExporter.data(), &aegis::utils::DiagnosticExporter::exportFinished,
            this, [](bool success, const QString& path, const QString& error) {
        if (success) {
            aegis::utils::Logger::instance().log(
                aegis::utils::LogLevel::Info, "Diagnostics",
                QStringLiteral("Bundle exported: %1").arg(path));
        } else {
            aegis::utils::Logger::instance().log(
                aegis::utils::LogLevel::Error, "Diagnostics",
                QStringLiteral("Export failed: %1").arg(error));
        }
    });

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

        // Tell the UI that the dummy telemetry stream is active.
        // Dummy telemetry simulates a fully alive link.
        m_bus->emitConnectionStateChanged(aegis::core::types::ConnectionState::HeartbeatAlive);
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
            this, [this](aegis::core::types::ConnectionState state) {
        m_bus->emitConnectionStateChanged(state);
    });

    connect(m_bus.data(), &aegis::core::TelemetryBus::connectionStateChanged,
            this, [](aegis::core::types::ConnectionState state) {
        QString msg;
        using S = aegis::core::types::ConnectionState;
        switch (state) {
            case S::Disconnected:      msg = "Disconnected"; break;
            case S::SocketBound:       msg = "SocketBound"; break;
            case S::VehicleDiscovered: msg = "VehicleDiscovered"; break;
            case S::HeartbeatAlive:    msg = "Connected"; break;
            case S::Degraded:          msg = "Degraded"; break;
            case S::Reconnecting:      msg = "Reconnecting"; break;
            case S::Error:             msg = "Error"; break;
            default:                   msg = "Unknown"; break;
        }
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
