#pragma once

#include <QMainWindow>
#include <QDockWidget>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QSettings>
#include <QCloseEvent>
#include <QHash>

#include "core/types/common.hpp"

namespace aegis::core { class IPlugin; }

namespace aegis::ui {

class DockManager;
class ConnectionBar;
class VehicleStatusBar;

/**
 * @brief Primary operator shell with persistent dockable layouts.
 *
 * Built on Qt6 QMainWindow with advanced docking. The central widget
 * hosts the primary map view; auxiliary plugins inhabit dock areas.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    DockManager* dockManager() const { return m_dockManager; }
    void injectPlugin(aegis::core::IPlugin* plugin);

    void restoreLayout();
    void saveLayout();

signals:
    void connectionRequested(const QString& host, quint16 port);
    void replayRequested(const QString& filePath);
    void pluginLoadRequested(const QString& pluginId);
    void aboutToShutdown();

public slots:
    void updateConnectionState(aegis::core::types::ConnectionState state);
    void updateSystemState(const aegis::core::types::SystemState& state);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildMenus();
    void buildToolbar();
    void buildStatusBar();

    DockManager* m_dockManager{nullptr};
    ConnectionBar* m_connectionBar{nullptr};
    VehicleStatusBar* m_vehicleStatusBar{nullptr};
    QHash<aegis::core::IPlugin*, QDockWidget*> m_pluginDocks;
    QSettings m_settings;
};

} // namespace aegis::ui
