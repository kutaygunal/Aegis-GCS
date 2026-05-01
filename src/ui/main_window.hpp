#pragma once

#include <QMainWindow>
#include <QDockWidget>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QSettings>
#include <QCloseEvent>

namespace aegis::ui {

class DockManager;

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

    void restoreLayout();
    void saveLayout();

signals:
    void connectionRequested(const QString& host, quint16 port);
    void replayRequested(const QString& filePath);
    void pluginLoadRequested(const QString& pluginId);
    void aboutToShutdown();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildMenus();
    void buildToolbar();
    void buildStatusBar();

    DockManager* m_dockManager{nullptr};
    QSettings m_settings;
};

} // namespace aegis::ui
