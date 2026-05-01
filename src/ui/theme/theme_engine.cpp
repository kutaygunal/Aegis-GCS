#include "theme_engine.hpp"
#include <QFile>
#include <QDebug>

namespace aegis::ui {

void ThemeEngine::applyDarkTheme(QWidget* target) {
    qApp->setStyleSheet(styleSheet());
    if (target) {
        target->setStyleSheet(styleSheet());
    }
}

void ThemeEngine::applyLightTheme(QWidget* target) {
    qApp->setStyleSheet(QString());
    Q_UNUSED(target)
    // TODO: define light-mode QSS
}

QString ThemeEngine::styleSheet() {
    return QStringLiteral(R"(
        QMainWindow, QWidget {
            background-color: %1;
            color: %2;
            font-family: "Segoe UI", "Roboto", sans-serif;
            font-size: 13px;
        }
        QDockWidget::title {
            background-color: %3;
            padding: 4px;
            font-weight: bold;
        }
        QToolBar {
            background-color: %3;
            border: none;
            spacing: 4px;
        }
        QStatusBar {
            background-color: %3;
            color: %2;
        }
        QPushButton {
            background-color: %3;
            border: 1px solid %2;
            padding: 6px 12px;
            border-radius: 2px;
        }
        QPushButton:hover {
            background-color: #37474F;
        }
        QPushButton:pressed {
            background-color: #455A64;
        }
        QLabel {
            color: %2;
        }
        QProgressBar {
            border: 1px solid %2;
            text-align: center;
            background-color: %1;
        }
        QProgressBar::chunk {
            background-color: %4;
        }
    )").arg(COLOR_BACKGROUND, COLOR_TEXT, COLOR_SURFACE, COLOR_GOOD);
}

} // namespace aegis::ui
