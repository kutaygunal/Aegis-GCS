add_library(map_view SHARED
    map_view_plugin.hpp
    map_view_plugin.cpp
    map_view.json
)

target_include_directories(map_view PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(map_view PRIVATE aegis_core Qt6::Core Qt6::Widgets)
set_target_properties(map_view PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)

if(TARGET Qt6::WebEngineWidgets)
    target_compile_definitions(map_view PRIVATE AEGIS_HAS_WEBENGINE)
    target_link_libraries(map_view PRIVATE Qt6::WebEngineWidgets)
endif()

add_custom_command(TARGET map_view POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:aegis>/plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:map_view>"
        "$<TARGET_FILE_DIR:aegis>/plugins/"
    COMMENT "Deploying map_view plugin..."
)
