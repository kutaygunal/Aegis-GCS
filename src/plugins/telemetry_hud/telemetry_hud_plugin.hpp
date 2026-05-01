#pragma once

#include "core/interfaces/itelemetry_sink.hpp"
#include "core/types/common.hpp"
#include <QLabel>
#include <QVBoxLayout>
#include <QProgressBar>

namespace aegis::plugins {

/**
 * @brief Real-time flight instrument panel.
 *
 * Displays attitude, altitude, speed, battery, and system status
 * using large operator-readable gauges suitable for high-stress scenarios.
 */
class TelemetryHudPlugin : public core::ITelemetrySink {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.aegis.gcs.IPlugin/1.0" FILE "telemetry_hud.json")
    Q_INTERFACES(aegis::core::IPlugin)

public:
    explicit TelemetryHudPlugin(QObject* parent = nullptr);
    ~TelemetryHudPlugin() override;

    bool initialize(core::TelemetryBus* bus, core::VehicleState* state,
                    const QVariantMap& config) override;

    QWidget* widget() override;
    void shutdown() override;

    QString name() const override      { return tr("Telemetry HUD"); }
    QString pluginId() const override  { return QStringLiteral("aegis.plugins.telemetry_hud"); }
    QString version() const override   { return QStringLiteral("1.0.0"); }
    QString author() const override    { return QStringLiteral("AEGIS Team"); }
    QString category() const override  { return QStringLiteral("Flight"); }

private slots:
    void onAttitudeChanged(const core::types::AttitudeData& data);
    void onBatteryChanged(const core::types::BatteryData& data);
    void onSystemStateChanged(const core::types::SystemState& state);

private:
    void buildUi();

    core::VehicleState* m_state{nullptr};

    QWidget* m_widget{nullptr};
    QLabel* m_rollLabel{nullptr};
    QLabel* m_pitchLabel{nullptr};
    QLabel* m_yawLabel{nullptr};
    QProgressBar* m_batteryBar{nullptr};
    QLabel* m_statusLabel{nullptr};
};

} // namespace aegis::plugins
