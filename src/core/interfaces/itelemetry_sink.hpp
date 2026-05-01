#pragma once

#include "iplugin.hpp"

namespace aegis::core {

/**
 * @brief Marker interface for plugins that primarily consume telemetry.
 *
 * Examples: HUD, Map, Alert Console, Sensor Feeds.
 */
class ITelemetrySink : public IPlugin {
    Q_OBJECT
public:
    ~ITelemetrySink() override = default;

protected:
    explicit ITelemetrySink(QObject* parent = nullptr) : IPlugin(parent) {}
};

} // namespace aegis::core

Q_DECLARE_INTERFACE(aegis::core::ITelemetrySink, AegisPluginInterface_iid)
