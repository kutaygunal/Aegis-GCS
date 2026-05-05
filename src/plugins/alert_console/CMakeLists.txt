add_library(alert_console SHARED
    alert_console_plugin.hpp
    alert_console_plugin.cpp
    alert_console.json
)

target_include_directories(alert_console PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(alert_console PRIVATE aegis_core Qt6::Core Qt6::Widgets)
set_target_properties(alert_console PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/$<CONFIG>
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/$<CONFIG>
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/plugins)
