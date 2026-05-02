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

void ConnectionBar::setConnected(bool connected) {
    m_statusLabel->setText(connected ? tr("Connected") : tr("Disconnected"));
    QString color = connected ? ThemeEngine::COLOR_GOOD : ThemeEngine::COLOR_CRITICAL;
    m_statusLabel->setStyleSheet(QStringLiteral("color: %1; font-weight: bold;").arg(color));
}

} // namespace aegis::ui
