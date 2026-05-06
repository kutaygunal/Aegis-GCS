add_library(map_view SHARED
    map_view_plugin.hpp
    map_view_plugin.cpp
    follow_vehicle_controller.hpp
    follow_vehicle_controller.cpp
    map_view.json
)

target_include_directories(map_view PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(map_view PRIVATE aegis_core aegis_map Qt6::Core Qt6::Widgets Qt6::Network)
set_target_properties(map_view PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/$<CONFIG>
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/$<CONFIG>
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/plugins)

if(TARGET Qt6::WebEngineWidgets)
    target_compile_definitions(map_view PRIVATE AEGIS_HAS_WEBENGINE)
    target_link_libraries(map_view PRIVATE Qt6::WebEngineWidgets)
endif()


