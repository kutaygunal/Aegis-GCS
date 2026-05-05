add_library(mission_editor SHARED
    mission_editor_plugin.hpp
    mission_editor_plugin.cpp
    mission_editor.json
)

target_include_directories(mission_editor PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(mission_editor PRIVATE aegis_core Qt6::Core Qt6::Widgets)
set_target_properties(mission_editor PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/$<CONFIG>
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/$<CONFIG>
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/plugins)
