add_library(telemetry_hud SHARED
    telemetry_hud_plugin.hpp
    telemetry_hud_plugin.cpp
    telemetry_hud.json
)

target_include_directories(telemetry_hud PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(telemetry_hud PRIVATE
    aegis_core
    Qt6::Core
    Qt6::Widgets
)

set_target_properties(telemetry_hud PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
)

add_custom_command(TARGET telemetry_hud POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:aegis>/plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:telemetry_hud>"
        "$<TARGET_FILE_DIR:aegis>/plugins/"
    COMMENT "Deploying telemetry_hud plugin..."
)
