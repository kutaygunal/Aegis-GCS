add_library(aegis_core STATIC
    interfaces/iplugin.hpp
    interfaces/itelemetry_sink.hpp
    interfaces/icommand_source.hpp
    bus/telemetry_bus.hpp
    bus/telemetry_bus.cpp
    state/vehicle_state.hpp
    state/vehicle_state.cpp
    plugin_host/plugin_host.hpp
    plugin_host/plugin_host.cpp
    plugin_host/plugin_meta.hpp
    plugin_host/plugin_meta.cpp
    types/common.hpp
)

target_include_directories(aegis_core PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(aegis_core PUBLIC
    Qt6::Core
    Qt6::Widgets
)

set_target_properties(aegis_core PROPERTIES
    EXPORT_NAME AegisCore
)
