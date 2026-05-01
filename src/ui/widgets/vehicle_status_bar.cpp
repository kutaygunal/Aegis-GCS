#include "vehicle_status_bar.hpp"
#include "theme/theme_engine.hpp"
#include <QHBoxLayout>

namespace aegis::ui {

VehicleStatusBar::VehicleStatusBar(QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    m_armedLabel = new QLabel(tr("DISARMED"));
    m_modeLabel = new QLabel(tr("MODE: STABILIZE"));
    layout->addWidget(m_armedLabel);
    layout->addStretch();
    layout->addWidget(m_modeLabel);
}

void VehicleStatusBar::setArmed(bool armed) {
    m_armedLabel->setText(armed ? tr("ARMED") : tr("DISARMED"));
    QString color = armed ? ThemeEngine::COLOR_CRITICAL : ThemeEngine::COLOR_GOOD;
    m_armedLabel->setStyleSheet(QStringLiteral("color: %1; font-weight: bold;").arg(color));
}

void VehicleStatusBar::setMode(const QString& mode) {
    m_modeLabel->setText(QStringLiteral("MODE: %1").arg(mode));
}

} // namespace aegis::ui
