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

add_executable(test_parsers
    telemetry/test_parsers.cpp
)
target_link_libraries(test_parsers PRIVATE
    aegis_core aegis_telemetry Qt6::Core Qt6::Test GTest::gtest_main)
add_test(NAME Parsers COMMAND test_parsers)

add_executable(test_log_replay
    telemetry/test_log_replay.cpp
)
target_link_libraries(test_log_replay PRIVATE
    aegis_core aegis_telemetry Qt6::Core Qt6::Test GTest::gtest_main)
add_test(NAME LogReplay COMMAND test_log_replay)

add_executable(test_connection_state_machine
    telemetry/test_connection_state_machine.cpp
)
target_link_libraries(test_connection_state_machine PRIVATE
    aegis_core aegis_telemetry Qt6::Core Qt6::Test GTest::gtest_main)
add_test(NAME ConnectionStateMachine COMMAND test_connection_state_machine)

add_executable(test_logger
    utils/test_logger.cpp
)
target_link_libraries(test_logger PRIVATE
    aegis_utils Qt6::Core Qt6::Test GTest::gtest_main)
add_test(NAME Logger COMMAND test_logger)

add_executable(test_mavlink_io
    telemetry/test_mavlink_io.cpp
)
target_link_libraries(test_mavlink_io PRIVATE
    aegis_core aegis_telemetry Qt6::Core Qt6::Network Qt6::Test GTest::gtest_main)
add_test(NAME MavlinkIO COMMAND test_mavlink_io)

add_executable(test_diagnostic_exporter
    utils/test_diagnostic_exporter.cpp
)
target_link_libraries(test_diagnostic_exporter PRIVATE
    aegis_core aegis_utils Qt6::Core Qt6::Test GTest::gtest_main)
add_test(NAME DiagnosticExporter COMMAND test_diagnostic_exporter)

add_executable(test_config_validator
    utils/test_config_validator.cpp
)
target_link_libraries(test_config_validator PRIVATE
    aegis_utils Qt6::Core Qt6::Test GTest::gtest_main)
add_test(NAME ConfigValidator COMMAND test_config_validator)
