#include "mission_editor_plugin.hpp"
#include "core/bus/telemetry_bus.hpp"
#include "core/types/common.hpp"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QDebug>

namespace aegis::plugins {

MissionEditorPlugin::MissionEditorPlugin(QObject* parent)
    : core::ICommandSource(parent) {}

MissionEditorPlugin::~MissionEditorPlugin() = default;

bool MissionEditorPlugin::initialize(core::TelemetryBus* bus,
                                       core::VehicleState* state,
                                       const QVariantMap& config) {
    Q_UNUSED(state) Q_UNUSED(config)
    m_bus = bus;
    buildUi();
    return true;
}

QWidget* MissionEditorPlugin::widget() { return m_widget; }

void MissionEditorPlugin::shutdown() {}

bool MissionEditorPlugin::canIssueCommand(const core::types::VehicleCommand& cmd) const {
    Q_UNUSED(cmd)
    return true;  // TODO: check permissions registry
}

void MissionEditorPlugin::buildUi() {
    m_widget = new QWidget();
    auto* vbox = new QVBoxLayout(m_widget);

    m_table = new QTableWidget(0, 4);
    m_table->setHorizontalHeaderLabels(
        QStringList() << tr("Index") << tr("Lat") << tr("Lon") << tr("Alt"));
    m_table->horizontalHeader()->setStretchLastSection(true);

    m_addBtn = new QPushButton(tr("Add Waypoint"));
    m_uploadBtn = new QPushButton(tr("Upload to Vehicle"));
    m_clearBtn = new QPushButton(tr("Clear All"));

    connect(m_addBtn, &QPushButton::clicked, this, &MissionEditorPlugin::onAddWaypoint);
    connect(m_uploadBtn, &QPushButton::clicked, this, &MissionEditorPlugin::onUploadMission);
    connect(m_clearBtn, &QPushButton::clicked, this, &MissionEditorPlugin::onClearMission);

    vbox->addWidget(m_table);
    vbox->addWidget(m_addBtn);
    vbox->addWidget(m_uploadBtn);
    vbox->addWidget(m_clearBtn);
}

void MissionEditorPlugin::onAddWaypoint() {
    int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setItem(row, 0, new QTableWidgetItem(QString::number(row)));
    m_table->setItem(row, 1, new QTableWidgetItem(QStringLiteral("0.0")));
    m_table->setItem(row, 2, new QTableWidgetItem(QStringLiteral("0.0")));
    m_table->setItem(row, 3, new QTableWidgetItem(QStringLiteral("50")));
}

void MissionEditorPlugin::onUploadMission() {
    for (int i = 0; i < m_table->rowCount(); ++i) {
        core::types::VehicleCommand cmd;
        cmd.commandId = 16;     // MAV_CMD_NAV_WAYPOINT
        cmd.targetSystem = 1;   // Default PX4 system ID
        cmd.targetComponent = 1;
        cmd.params[0] = 0;      // Hold time
        cmd.params[1] = 0;      // Accept radius
        cmd.params[2] = 0;      // Pass radius
        cmd.params[3] = std::numeric_limits<float>::quiet_NaN(); // Yaw

        bool ok;
        double lat = m_table->item(i, 1)->text().toDouble(&ok);
        double lon = m_table->item(i, 2)->text().toDouble(&ok);
        double alt = m_table->item(i, 3)->text().toDouble(&ok);

        cmd.params[4] = static_cast<float>(lat);
        cmd.params[5] = static_cast<float>(lon);
        cmd.params[6] = static_cast<float>(alt);

        if (m_bus && canIssueCommand(cmd)) {
            m_bus->postCommand(cmd);
            qDebug() << "[MissionEditor] Sent NAV_WAYPOINT" << i
                     << "lat:" << cmd.params[4] << "lon:" << cmd.params[5];
        }
    }
}

void MissionEditorPlugin::onClearMission() {
    m_table->setRowCount(0);
}

} // namespace aegis::plugins
