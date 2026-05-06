add_library(aegis_map STATIC
    tile_types.hpp
    tile_cache.hpp
    tile_loader.hpp
    tile_loader.cpp
)

target_include_directories(aegis_map PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(aegis_map PUBLIC
    Qt6::Core
    Qt6::Network
    aegis_core
)

set_target_properties(aegis_map PROPERTIES
    EXPORT_NAME AegisMap
)
