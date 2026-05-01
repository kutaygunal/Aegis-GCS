#include "dock_manager.hpp"
#include "core/interfaces/iplugin.hpp"
#include <QDebug>

namespace aegis::ui {

DockManager::DockManager(QWidget* parent) : QMainWindow(parent) {
    setDockNestingEnabled(true);
}

QDockWidget* DockManager::injectPlugin(aegis::core::IPlugin* plugin) {
    if (!plugin || !plugin->widget()) return nullptr;
    if (m_docks.contains(plugin)) return m_docks.value(plugin);

    auto* dock = new QDockWidget(plugin->name(), this);
    dock->setWidget(plugin->widget());
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    dock->setFeatures(QDockWidget::DockWidgetMovable |
                       QDockWidget::DockWidgetFloatable |
                       QDockWidget::DockWidgetClosable);

    addDockWidget(Qt::RightDockWidgetArea, dock);
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
