add_library(aegis_ui STATIC
    main_window.hpp
    main_window.cpp
    dock_manager.hpp
    dock_manager.cpp
    theme/theme_engine.hpp
    theme/theme_engine.cpp
    widgets/connection_bar.hpp
    widgets/connection_bar.cpp
    widgets/vehicle_status_bar.hpp
    widgets/vehicle_status_bar.cpp
)

target_include_directories(aegis_ui PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(aegis_ui PUBLIC
    aegis_core
    Qt6::Core
    Qt6::Widgets
    Qt6::WebEngineWidgets
)
