#include "alert_console_plugin.hpp"
#include "core/bus/telemetry_bus.hpp"
#include "ui/theme/theme_engine.hpp"
#include <QVBoxLayout>
#include <QLabel>
#include <QDateTime>

namespace aegis::plugins {

AlertConsolePlugin::AlertConsolePlugin(QObject* parent)
    : core::ITelemetrySink(parent) {}

AlertConsolePlugin::~AlertConsolePlugin() = default;

bool AlertConsolePlugin::initialize(core::TelemetryBus* bus,
                                     core::VehicleState* state,
                                     const QVariantMap& config) {
    Q_UNUSED(state)
    m_maxItems = qMax(1, config.value("maxItems", 200).toInt());
    m_showTimestamps = config.value("showTimestamps", true).toBool();
    buildUi();
    connect(bus, &core::TelemetryBus::alertRaised,
            this, &AlertConsolePlugin::onAlert,
            Qt::QueuedConnection);
    return true;
}

QWidget* AlertConsolePlugin::widget() { return m_widget; }

void AlertConsolePlugin::shutdown() {}

void AlertConsolePlugin::buildUi() {
    m_widget = new QWidget();
    auto* vbox = new QVBoxLayout(m_widget);
    vbox->setContentsMargins(4, 4, 4, 4);

    auto* header = new QLabel(tr("System Alerts"));
    header->setStyleSheet(QStringLiteral("font-weight: bold; color: %1;")
                             .arg(aegis::ui::ThemeEngine::COLOR_TEXT));
    vbox->addWidget(header);

    m_list = new QListWidget();
    m_list->setWordWrap(true);
    vbox->addWidget(m_list);
}

void AlertConsolePlugin::onAlert(core::types::AlertLevel level,
                                  const QString& message) {
    QString color = aegis::ui::ThemeEngine::COLOR_INFO;
    switch (level) {
        case core::types::AlertLevel::Warning:  color = aegis::ui::ThemeEngine::COLOR_WARNING; break;
        case core::types::AlertLevel::Critical: color = aegis::ui::ThemeEngine::COLOR_CRITICAL; break;
        case core::types::AlertLevel::Emergency: color = aegis::ui::ThemeEngine::COLOR_CRITICAL; break;
        default: break;
    }

    QString text = message;
    if (m_showTimestamps) {
        const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
        text = QStringLiteral("[%1] %2").arg(timestamp, message);
    }

    auto* item = new QListWidgetItem(text);
    item->setForeground(QColor(color));
    m_list->addItem(item);
    while (m_list->count() > m_maxItems) {
        delete m_list->takeItem(0);
    }
    m_list->scrollToBottom();
}

} // namespace aegis::plugins
