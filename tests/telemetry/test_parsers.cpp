#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QDateTime>
#include "core/bus/telemetry_bus.hpp"
#include "core/state/vehicle_state.hpp"
#include "telemetry/parsers.hpp"
#include <mavlink.h>

using namespace aegis;

class MavlinkParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        app = new QCoreApplication(argc, argv);
        bus = new core::TelemetryBus();
        state = new core::VehicleState();
        parser = new telemetry::MavlinkParser(bus, state);
    }

    void TearDown() override {
        delete parser;
        delete state;
        delete bus;
        delete app;
    }

    telemetry::types::MavlinkMessage wrap(const mavlink_message_t& msg) {
        telemetry::types::MavlinkMessage wrapped;
        wrapped.raw = msg;
        wrapped.timestamp = QDateTime::currentDateTimeUtc();
        return wrapped;
    }

    int argc = 1;
    char* argv[1] = {const_cast<char*>("test")};
    QCoreApplication* app{nullptr};
    core::TelemetryBus* bus{nullptr};
    core::VehicleState* state{nullptr};
    telemetry::MavlinkParser* parser{nullptr};
};

TEST_F(MavlinkParserTest, ParsesHeartbeat) {
    mavlink_message_t msg{};
    mavlink_msg_heartbeat_pack(1, 1, &msg,
                               MAV_TYPE_QUADROTOR,
                               MAV_AUTOPILOT_PX4,
                               MAV_MODE_FLAG_MANUAL_INPUT_ENABLED | MAV_MODE_FLAG_SAFETY_ARMED,
                               42,
                               MAV_STATE_ACTIVE);

    parser->processMessage(wrap(msg));

    const auto system = state->systemState();
    EXPECT_EQ(system.systemId, 1);
    EXPECT_TRUE(system.armed);
    EXPECT_EQ(system.customMode, 42u);
    EXPECT_EQ(system.status, core::types::SystemStatus::Active);
}

TEST_F(MavlinkParserTest, ParsesAttitude) {
    mavlink_message_t msg{};
    mavlink_msg_attitude_pack(1, 1, &msg, 1000, 0.1f, -0.2f, 0.3f, 0.4f, 0.5f, 0.6f);

    parser->processMessage(wrap(msg));

    const auto attitude = state->attitude();
    EXPECT_NEAR(attitude.roll, 0.1, 1e-6);
    EXPECT_NEAR(attitude.pitch, -0.2, 1e-6);
    EXPECT_NEAR(attitude.yaw, 0.3, 1e-6);
    EXPECT_NEAR(attitude.rollSpeed, 0.4, 1e-6);
}

TEST_F(MavlinkParserTest, ParsesGlobalPosition) {
    mavlink_message_t msg{};
    mavlink_msg_global_position_int_pack(1, 1, &msg, 1000,
                                         374221234, -1220845678,
                                         123000, 45000,
                                         120, -30, 5, 27000);

    parser->processMessage(wrap(msg));

    const auto pos = state->position();
    EXPECT_EQ(pos.lat, 374221234);
    EXPECT_EQ(pos.lon, -1220845678);
    EXPECT_EQ(pos.alt, 123000);
    EXPECT_EQ(pos.relativeAlt, 45000);
    EXPECT_EQ(pos.hdg, 27000);
}

TEST_F(MavlinkParserTest, ParsesBatteryStatus) {
    mavlink_message_t msg{};
    uint16_t voltages[10] = {1480, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
                             UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX};
    uint16_t voltagesExt[4] = {0, 0, 0, 0};
    mavlink_msg_battery_status_pack(1, 1, &msg,
                                    0,
                                    MAV_BATTERY_FUNCTION_ALL,
                                    MAV_BATTERY_TYPE_LIPO,
                                    250,
                                    voltages,
                                    250,
                                    -1,
                                    -1,
                                    80,
                                    0,
                                    MAV_BATTERY_CHARGE_STATE_OK,
                                    voltagesExt,
                                    0,
                                    0);

    parser->processMessage(wrap(msg));

    const auto battery = state->battery();
    EXPECT_EQ(battery.remaining, 80);
    EXPECT_NEAR(battery.voltageV, 14.8, 1e-6);
    EXPECT_NEAR(battery.currentA, 2.5, 1e-6);
}
