#include "connection_bar.hpp"
#include "../theme/theme_engine.hpp"
#include <QHBoxLayout>

namespace aegis::ui {

ConnectionBar::ConnectionBar(QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    m_statusLabel = new QLabel(tr("Disconnected"));
    layout->addWidget(m_statusLabel);
    layout->addStretch();
}

void ConnectionBar::setConnectionState(aegis::core::types::ConnectionState state) {
    QString text;
    QString color;

    using State = aegis::core::types::ConnectionState;

    switch (state) {
        case State::Disconnected:
            text = tr("Disconnected");
            color = ThemeEngine::COLOR_CRITICAL;
            break;
        case State::SocketBound:
            text = tr("Socket Bound");
            color = ThemeEngine::COLOR_WARNING;
            break;
        case State::VehicleDiscovered:
            text = tr("Vehicle Discovered");
            color = ThemeEngine::COLOR_WARNING;
            break;
        case State::HeartbeatAlive:
            text = tr("Connected");
            color = ThemeEngine::COLOR_GOOD;
            break;
        case State::Degraded:
            text = tr("Degraded");
            color = ThemeEngine::COLOR_WARNING;
            break;
        case State::Reconnecting:
            text = tr("Reconnecting");
            color = ThemeEngine::COLOR_WARNING;
            break;
        case State::Error:
            text = tr("Error");
            color = ThemeEngine::COLOR_CRITICAL;
            break;
        default:
            text = tr("Unknown");
            color = ThemeEngine::COLOR_CRITICAL;
            break;
    }

    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(QStringLiteral("color: %1; font-weight: bold;").arg(color));
}

} // namespace aegis::ui
