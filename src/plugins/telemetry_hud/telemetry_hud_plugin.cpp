#include "telemetry_hud_plugin.hpp"
#include "core/bus/telemetry_bus.hpp"
#include "core/state/vehicle_state.hpp"
#include "ui/theme/theme_engine.hpp"
#include <QDebug>

namespace aegis::plugins {

TelemetryHudPlugin::TelemetryHudPlugin(QObject* parent)
    : core::ITelemetrySink(parent) {}

TelemetryHudPlugin::~TelemetryHudPlugin() = default;

bool TelemetryHudPlugin::initialize(core::TelemetryBus* bus,
                                     core::VehicleState* state,
                                     const QVariantMap& config) {
    Q_UNUSED(config)
    m_state = state;
    buildUi();

    connect(bus, &core::TelemetryBus::attitudeChanged,
            this, &TelemetryHudPlugin::onAttitudeChanged,
            Qt::QueuedConnection);
    connect(bus, &core::TelemetryBus::batteryChanged,
            this, &TelemetryHudPlugin::onBatteryChanged,
            Qt::QueuedConnection);
    connect(state, &core::VehicleState::systemStateChanged,
            this, &TelemetryHudPlugin::onSystemStateChanged,
            Qt::QueuedConnection);

    return true;
}

QWidget* TelemetryHudPlugin::widget() {
    return m_widget;
}

void TelemetryHudPlugin::shutdown() {
    // Unsubscriptions handled by QObject::~QObject
}

void TelemetryHudPlugin::buildUi() {
    m_widget = new QWidget();
    auto* vbox = new QVBoxLayout(m_widget);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(6);

    m_rollLabel  = new QLabel(tr("Roll:  ---"));
    m_pitchLabel = new QLabel(tr("Pitch: ---"));
    m_yawLabel   = new QLabel(tr("Yaw:   ---"));
    m_batteryBar = new QProgressBar();
    m_batteryBar->setRange(0, 100);
    m_batteryBar->setValue(0);
    m_statusLabel = new QLabel(tr("Status: Unknown"));

    for (auto* lbl : {m_rollLabel, m_pitchLabel, m_yawLabel, m_statusLabel}) {
        lbl->setStyleSheet(QString("font-size: 16px; font-weight: bold; color: %1;")
                              .arg(aegis::ui::ThemeEngine::COLOR_TEXT));
    }

    vbox->addWidget(m_rollLabel);
    vbox->addWidget(m_pitchLabel);
    vbox->addWidget(m_yawLabel);
    vbox->addWidget(m_batteryBar);
    vbox->addWidget(m_statusLabel);
    vbox->addStretch();
}

void TelemetryHudPlugin::onAttitudeChanged(const core::types::AttitudeData& data) {
    m_rollLabel->setText(QStringLiteral("Roll:  %1°").arg(qRadiansToDegrees(data.roll), 0, 'f', 1));
    m_pitchLabel->setText(QStringLiteral("Pitch: %1°").arg(qRadiansToDegrees(data.pitch), 0, 'f', 1));
    m_yawLabel->setText(QStringLiteral("Yaw:   %1°").arg(qRadiansToDegrees(data.yaw), 0, 'f', 1));
}

void TelemetryHudPlugin::onBatteryChanged(const core::types::BatteryData& data) {
    if (data.remaining >= 0) {
        m_batteryBar->setValue(data.remaining);
        QString color = aegis::ui::ThemeEngine::COLOR_GOOD;
        if (data.remaining < 30)  color = aegis::ui::ThemeEngine::COLOR_WARNING;
        if (data.remaining < 15)  color = aegis::ui::ThemeEngine::COLOR_CRITICAL;
        m_batteryBar->setStyleSheet(
            QStringLiteral("QProgressBar::chunk { background-color: %1; }").arg(color));
    }
}

void TelemetryHudPlugin::onSystemStateChanged(const core::types::SystemState& state) {
    QString status;
    switch (state.status) {
        case core::types::SystemStatus::Standby: status = tr("Standby"); break;
        case core::types::SystemStatus::Active:  status = tr("Active"); break;
        case core::types::SystemStatus::Critical: status = tr("CRITICAL"); break;
        case core::types::SystemStatus::Emergency: status = tr("EMERGENCY"); break;
        default: status = tr("Unknown"); break;
    }
    m_statusLabel->setText(QStringLiteral("Status: %1 | Armed: %2")
                               .arg(status, state.armed ? tr("YES") : tr("NO")));
}

} // namespace aegis::plugins
