#pragma once

#include "core/interfaces/itelemetry_sink.hpp"
#include "core/types/common.hpp"
#include <QWidget>
#include <QListWidget>

namespace aegis::plugins {

/**
 * @brief Scrollable alert console with severity coloring and filtering.
 */
class AlertConsolePlugin : public core::ITelemetrySink {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.aegis.gcs.IPlugin/1.0" FILE "alert_console.json")
    Q_INTERFACES(aegis::core::IPlugin)

public:
    explicit AlertConsolePlugin(QObject* parent = nullptr);
    ~AlertConsolePlugin() override;

    bool initialize(core::TelemetryBus* bus, core::VehicleState* state,
                    const QVariantMap& config) override;
    QWidget* widget() override;
    void shutdown() override;

    QString name() const override    { return tr("Alert Console"); }
    QString pluginId() const override { return QStringLiteral("aegis.plugins.alert_console"); }
    QString version() const override { return QStringLiteral("1.0.0"); }
    QString author() const override  { return QStringLiteral("AEGIS Team"); }
    QString category() const override{ return QStringLiteral("Diagnostics"); }

private slots:
    void onAlert(core::types::AlertLevel level, const QString& message);

private:
    void buildUi();
    QListWidget* m_list{nullptr};
    QWidget* m_widget{nullptr};
};

} // namespace aegis::plugins
