#include "dock_manager.hpp"
#include "core/interfaces/iplugin.hpp"
#include <QDebug>

namespace aegis::ui {

DockManager::DockManager(QWidget* parent) : QMainWindow(parent) {
    setDockNestingEnabled(true);

    auto* center = new QWidget(this);
    center->setObjectName(QStringLiteral("dockCenter"));
    setCentralWidget(center);
}

QDockWidget* DockManager::injectPlugin(aegis::core::IPlugin* plugin) {
    if (!plugin || !plugin->widget()) return nullptr;
    if (m_docks.contains(plugin)) return m_docks.value(plugin);

    const bool isMapView = (plugin->pluginId() == QStringLiteral("aegis.plugins.map_view"));

    if (isMapView) {
        if (auto* oldCenter = centralWidget(); oldCenter && oldCenter != plugin->widget()) {
            oldCenter->deleteLater();
        }
        setCentralWidget(plugin->widget());
        plugin->widget()->show();
        plugin->widget()->raise();
        emit pluginFocused(plugin);
        return nullptr;
    }

    auto* dock = new QDockWidget(plugin->name(), this);
    dock->setObjectName(plugin->pluginId());
    dock->setWidget(plugin->widget());
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    dock->setFeatures(QDockWidget::DockWidgetMovable |
                       QDockWidget::DockWidgetFloatable |
                       QDockWidget::DockWidgetClosable);

    addDockWidget(Qt::RightDockWidgetArea, dock);

    for (auto it = m_docks.constBegin(); it != m_docks.constEnd(); ++it) {
        if (it.value()) {
            tabifyDockWidget(it.value(), dock);
            break;
        }
    }

    dock->show();
    dock->raise();
    m_docks.insert(plugin, dock);

    connect(dock, &QDockWidget::visibilityChanged, this, [this, plugin](bool visible) {
        if (visible) emit pluginFocused(plugin);
    });

    return dock;
}

void DockManager::ejectPlugin(aegis::core::IPlugin* plugin) {
    auto* dock = m_docks.take(plugin);
    if (dock) {
        removeDockWidget(dock);
        dock->deleteLater();
    }
}

void DockManager::autoTabify(const QString& category) {
    // TODO: group dock widgets by plugin category and tabify them
    Q_UNUSED(category)
}

} // namespace aegis::ui
