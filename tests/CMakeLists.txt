find_package(Qt6 REQUIRED COMPONENTS Test)
find_package(GTest REQUIRED)

set(TEST_COMMON
    ../src/core/types/common.hpp
    ../src/core/bus/telemetry_bus.cpp
    ../src/core/state/vehicle_state.cpp
    ../src/utils/logging.cpp
)

# ── Unit tests ──────────────────────────────────────────────────────
add_executable(test_telemetry_bus
    core/test_telemetry_bus.cpp
    ${TEST_COMMON}
)
target_link_libraries(test_telemetry_bus PRIVATE
    aegis_core Qt6::Core Qt6::Test GTest::gtest_main)
add_test(NAME TelemetryBus COMMAND test_telemetry_bus)

add_executable(test_vehicle_state
    core/test_vehicle_state.cpp
    ${TEST_COMMON}
)
target_link_libraries(test_vehicle_state PRIVATE
    aegis_core Qt6::Core Qt6::Test GTest::gtest_main)
add_test(NAME VehicleState COMMAND test_vehicle_state)

add_executable(test_ring_buffer
    utils/test_ring_buffer.cpp
    ../src/utils/ring_buffer.hpp
)
target_link_libraries(test_ring_buffer PRIVATE Qt6::Core GTest::gtest_main)
add_test(NAME RingBuffer COMMAND test_ring_buffer)
