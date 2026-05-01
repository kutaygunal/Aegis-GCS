#include "app/application.hpp"
#include <QApplication>
#include <QSurfaceFormat>

int main(int argc, char* argv[]) {
    // Enable high-DPI scaling and OpenGL multisampling
    QSurfaceFormat fmt;
    fmt.setSamples(4);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    app.setApplicationName("AEGIS GCS");
    app.setOrganizationName("Aegis Aerospace");
    app.setApplicationVersion("0.1.0");

    aegis::app::Application aegis;
    if (!aegis.initialize()) {
        return 1;
    }
    aegis.showWindow();

    return app.exec();
}
