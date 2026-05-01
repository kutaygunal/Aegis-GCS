#pragma once

#include "iplugin.hpp"
#include "../types/common.hpp"

namespace aegis::core {

/**
 * @brief Marker interface for plugins that issue vehicle commands.
 *
 * Examples: Mission Editor, Joystick controller, Command Console.
 */
class ICommandSource : public IPlugin {
    Q_OBJECT
public:
    ~ICommandSource() override = default;

    /** @return true if this plugin is authorized to issue the given command. */
    virtual bool canIssueCommand(const types::VehicleCommand& cmd) const = 0;

signals:
    /** @brief Emitted when the plugin wants to send a command to the vehicle. */
    void commandRequested(const types::VehicleCommand& cmd);

protected:
    explicit ICommandSource(QObject* parent = nullptr) : IPlugin(parent) {}
};

} // namespace aegis::core

Q_DECLARE_INTERFACE(aegis::core::ICommandSource, AegisPluginInterface_iid)
