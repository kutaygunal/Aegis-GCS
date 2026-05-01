#include "main_window.hpp"
#include "dock_manager.hpp"
#include "theme/theme_engine.hpp"
#include "core/interfaces/iplugin.hpp"
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>

namespace aegis::ui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("AEGIS GCS v0.1.0"));
    resize(1600, 900);

    m_dockManager = new DockManager(this);
    setCentralWidget(m_dockManager);

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
    restoreState(m_settings.value("state").toByteArray());
    m_settings.endGroup();
}

void MainWindow::saveLayout() {
    m_settings.beginGroup("MainWindow");
    m_settings.setValue("geometry", saveGeometry());
    m_settings.setValue("state", saveState());
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
    auto* status = new QLabel(tr("Disconnected | No Vehicle"));
    statusBar()->addWidget(status);
}

} // namespace aegis::ui
