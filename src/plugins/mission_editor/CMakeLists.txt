add_library(mission_editor SHARED
    mission_editor_plugin.hpp
    mission_editor_plugin.cpp
    mission_editor.json
)

target_include_directories(mission_editor PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(mission_editor PRIVATE aegis_core Qt6::Core Qt6::Widgets)
set_target_properties(mission_editor PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)

add_custom_command(TARGET mission_editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:aegis>/plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:mission_editor>"
        "$<TARGET_FILE_DIR:aegis>/plugins/"
    COMMENT "Deploying mission_editor plugin..."
)
