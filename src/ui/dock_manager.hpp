#pragma once

#include <QMainWindow>
#include <QDockWidget>
#include <QMap>
#include <QHash>

namespace aegis::core { class IPlugin; }

namespace aegis::ui {

/**
 * @brief Wraps QMainWindow dock areas to inject plugin widgets safely.
 *
 * Handles tabification, floating restrictions, and layout serialization.
 */
class DockManager : public QMainWindow {
    Q_OBJECT

public:
    explicit DockManager(QWidget* parent = nullptr);

    /** @brief Inject a plugin widget into the next available dock area. */
    QDockWidget* injectPlugin(aegis::core::IPlugin* plugin);

    /** @brief Remove and destroy a plugin's dock container. */
    void ejectPlugin(aegis::core::IPlugin* plugin);

    /** @brief Tabify all plugins in the same category together. */
    void autoTabify(const QString& category);

signals:
    void pluginFocused(aegis::core::IPlugin* plugin);

private:
    QHash<aegis::core::IPlugin*, QDockWidget*> m_docks;
};

} // namespace aegis::ui
