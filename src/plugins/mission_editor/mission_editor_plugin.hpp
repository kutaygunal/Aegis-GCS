#pragma once

#include "core/interfaces/icommand_source.hpp"
#include <QWidget>
#include <QTableWidget>
#include <QPushButton>

namespace aegis::plugins {

/**
 * @brief Waypoint-based mission editor with upload capability.
 */
class MissionEditorPlugin : public core::ICommandSource {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.aegis.gcs.IPlugin/1.0" FILE "mission_editor.json")
    Q_INTERFACES(aegis::core::IPlugin)

public:
    explicit MissionEditorPlugin(QObject* parent = nullptr);
    ~MissionEditorPlugin() override;

    bool initialize(core::TelemetryBus* bus, core::VehicleState* state,
                    const QVariantMap& config) override;
    QWidget* widget() override;
    void shutdown() override;

    QString name() const override    { return tr("Mission Editor"); }
    QString pluginId() const override { return QStringLiteral("aegis.plugins.mission_editor"); }
    QString version() const override { return QStringLiteral("1.0.0"); }
    QString author() const override  { return QStringLiteral("AEGIS Team"); }
    QString category() const override{ return QStringLiteral("Mission"); }

    bool canIssueCommand(const core::types::VehicleCommand& cmd) const override;

private slots:
    void onAddWaypoint();
    void onUploadMission();
    void onClearMission();

private:
    void buildUi();

    QWidget* m_widget{nullptr};
    QTableWidget* m_table{nullptr};
    QPushButton* m_addBtn{nullptr};
    QPushButton* m_uploadBtn{nullptr};
    QPushButton* m_clearBtn{nullptr};
    core::TelemetryBus* m_bus{nullptr};
};

} // namespace aegis::plugins
