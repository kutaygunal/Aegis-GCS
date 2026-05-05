#include "main_window.hpp"
#include "dock_manager.hpp"
#include "theme/theme_engine.hpp"
#include "core/interfaces/iplugin.hpp"
#include "widgets/connection_bar.hpp"
#include "widgets/vehicle_status_bar.hpp"
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>

#include <QLabel>

namespace aegis::ui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("AEGIS GCS v0.1.0"));
    resize(1600, 900);

    m_dockManager = new DockManager(this);
    auto* emptyCenter = new QWidget(this);
    emptyCenter->setObjectName(QStringLiteral("mainEmptyCenter"));
    setCentralWidget(emptyCenter);

    buildMenus();
    buildToolbar();
    buildStatusBar();

    ThemeEngine::applyDarkTheme(this);
    restoreLayout();
}

MainWindow::~MainWindow() {
    saveLayout();
}

void MainWindow::restoreLayout() {
    m_settings.beginGroup("MainWindow");
    restoreGeometry(m_settings.value("geometry").toByteArray());
    // Dock/plugin layout restore disabled for now because older saved states
    // can hide newly injected plugin panels and central widgets.
    m_settings.endGroup();
}

void MainWindow::saveLayout() {
    m_settings.beginGroup("MainWindow");
    m_settings.setValue("geometry", saveGeometry());
    m_settings.remove("state");
    m_settings.endGroup();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    emit aboutToShutdown();
    event->accept();
}

void MainWindow::buildMenus() {
    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    auto* openReplay = fileMenu->addAction(tr("Open &Replay..."));
    connect(openReplay, &QAction::triggered, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, tr("Open Replay"),
                                                   QString(), tr("TLogs (*.tlog)"));
        if (!path.isEmpty()) emit replayRequested(path);
    });
    fileMenu->addSeparator();
    auto* exitAction = fileMenu->addAction(tr("E&xit"));
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    auto* connectMenu = menuBar()->addMenu(tr("&Connect"));
    auto* connectUdp = connectMenu->addAction(tr("UDP MAVLink..."));
    connect(connectUdp, &QAction::triggered, this, [this]() {
        emit connectionRequested(QStringLiteral("0.0.0.0"), 14550);
    });

    auto* pluginsMenu = menuBar()->addMenu(tr("&Plugins"));
    // Dynamically populated by Application layer
    Q_UNUSED(pluginsMenu)
}

void MainWindow::buildToolbar() {
    auto* toolbar = addToolBar(tr("Main"));
    toolbar->setObjectName(QStringLiteral("mainToolbar"));
    toolbar->setMovable(false);
    auto* connectAction = toolbar->addAction(tr("Connect"));
    connect(connectAction, &QAction::triggered, this, [this]() {
        emit connectionRequested(QStringLiteral("0.0.0.0"), 14550);
    });
    toolbar->addSeparator();
    auto* replayAction = toolbar->addAction(tr("Replay"));
    connect(replayAction, &QAction::triggered, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, tr("Open Replay"));
        if (!path.isEmpty()) emit replayRequested(path);
    });
}

void MainWindow::buildStatusBar() {
    m_connectionBar = new ConnectionBar(this);
    m_vehicleStatusBar = new VehicleStatusBar(this);
    statusBar()->addWidget(m_connectionBar);
    statusBar()->addPermanentWidget(m_vehicleStatusBar);
}

void MainWindow::updateConnectionState(aegis::core::types::ConnectionState state) {
    if (m_connectionBar) {
        m_connectionBar->setConnected(state == aegis::core::types::ConnectionState::Connected);
    }
}

void MainWindow::updateSystemState(const aegis::core::types::SystemState& state) {
    if (m_vehicleStatusBar) {
        m_vehicleStatusBar->setArmed(state.armed);
        m_vehicleStatusBar->setMode("MANUAL");
    }
}

void MainWindow::injectPlugin(aegis::core::IPlugin* plugin) {
    if (!plugin || !plugin->widget()) return;

    const bool isMapView = (plugin->pluginId() == QStringLiteral("aegis.plugins.map_view"));
    if (isMapView) {
        if (auto* oldCenter = centralWidget(); oldCenter && oldCenter != plugin->widget()) {
            oldCenter->deleteLater();
        }
        setCentralWidget(plugin->widget());
        plugin->widget()->show();
        plugin->widget()->raise();
        return;
    }

    if (m_pluginDocks.contains(plugin)) {
        if (auto* dock = m_pluginDocks.value(plugin)) {
            dock->show();
            dock->raise();
        }
        return;
    }

    auto* dock = new QDockWidget(plugin->name(), this);
    dock->setObjectName(plugin->pluginId());
    dock->setWidget(plugin->widget());
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    dock->setFeatures(QDockWidget::DockWidgetMovable |
                      QDockWidget::DockWidgetFloatable |
                      QDockWidget::DockWidgetClosable);

    addDockWidget(Qt::RightDockWidgetArea, dock);
    if (!m_pluginDocks.isEmpty()) {
        tabifyDockWidget(m_pluginDocks.constBegin().value(), dock);
    }
    dock->show();
    dock->raise();
    m_pluginDocks.insert(plugin, dock);
}

} // namespace aegis::ui
