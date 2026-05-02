add_library(alert_console SHARED
    alert_console_plugin.hpp
    alert_console_plugin.cpp
    alert_console.json
)

target_include_directories(alert_console PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(alert_console PRIVATE aegis_core Qt6::Core Qt6::Widgets)
set_target_properties(alert_console PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)

add_custom_command(TARGET alert_console POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:aegis>/plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:alert_console>"
        "$<TARGET_FILE_DIR:aegis>/plugins/"
    COMMENT "Deploying alert_console plugin..."
)
