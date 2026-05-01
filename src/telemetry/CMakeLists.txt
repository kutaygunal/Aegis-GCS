add_library(aegis_telemetry STATIC
    mavlink_io.hpp
    mavlink_io.cpp
    parsers.hpp
    parsers.cpp
    replay/log_replay.hpp
    replay/log_replay.cpp
    types/mavlink_types.hpp
)

target_include_directories(aegis_telemetry PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party  # MAVLink C library
)

target_link_libraries(aegis_telemetry PUBLIC
    aegis_core
    Qt6::Core
    Qt6::Network
)
