add_library(aegis_utils STATIC
    logging.hpp
    logging.cpp
    ring_buffer.hpp
    thread_pool.hpp
    thread_pool.cpp
)

target_include_directories(aegis_utils PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(aegis_utils PUBLIC Qt6::Core)
