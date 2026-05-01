#include "mission_editor_plugin.hpp"
#include "core/bus/telemetry_bus.hpp"
#include <QVBoxLayout>
#include <QHeaderView>

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
    // TODO: marshal waypoints into MAVLink MISSION_ITEM_INT messages
}

void MissionEditorPlugin::onClearMission() {
    m_table->setRowCount(0);
}

} // namespace aegis::plugins
