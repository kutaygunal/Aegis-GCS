#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QTemporaryFile>
#include <QSignalSpy>
#include "core/bus/telemetry_bus.hpp"
#include "core/state/vehicle_state.hpp"
#include "telemetry/parsers.hpp"
#include "telemetry/replay/log_replay.hpp"
#include <mavlink.h>

using namespace aegis;

class LogReplayTest : public ::testing::Test {
protected:
    void SetUp() override {
        app = new QCoreApplication(argc, argv);
        bus = new core::TelemetryBus();
        state = new core::VehicleState();
        parser = new telemetry::MavlinkParser(bus, state);
        replay = new telemetry::LogReplay(bus);
        QObject::connect(replay, &telemetry::LogReplay::messageReplayed,
                         parser, &telemetry::MavlinkParser::processMessage);
    }

    void TearDown() override {
        delete replay;
        delete parser;
        delete state;
        delete bus;
        delete app;
    }

    QByteArray encodeHeartbeat() {
        mavlink_message_t msg{};
        mavlink_msg_heartbeat_pack(1, 1, &msg,
                                   MAV_TYPE_QUADROTOR,
                                   MAV_AUTOPILOT_PX4,
                                   MAV_MODE_FLAG_MANUAL_INPUT_ENABLED | MAV_MODE_FLAG_SAFETY_ARMED,
                                   7,
                                   MAV_STATE_ACTIVE);
        uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
        const auto len = mavlink_msg_to_send_buffer(buffer, &msg);
        return QByteArray(reinterpret_cast<const char*>(buffer), len);
    }

    int argc = 1;
    char* argv[1] = {const_cast<char*>("test")};
    QCoreApplication* app{nullptr};
    core::TelemetryBus* bus{nullptr};
    core::VehicleState* state{nullptr};
    telemetry::MavlinkParser* parser{nullptr};
    telemetry::LogReplay* replay{nullptr};
};

TEST_F(LogReplayTest, StepReplaysSingleMessage) {
    QTemporaryFile file;
    ASSERT_TRUE(file.open());
    file.write(encodeHeartbeat());
    file.flush();
    file.close();

    ASSERT_TRUE(replay->loadFile(file.fileName()));
    replay->step();
    QCoreApplication::processEvents();

    EXPECT_TRUE(state->systemState().armed);
    EXPECT_GT(replay->progress(), 0.0);
}

TEST_F(LogReplayTest, PlaybackFinishesAndEmitsProgress) {
    QTemporaryFile file;
    ASSERT_TRUE(file.open());
    file.write(encodeHeartbeat());
    file.write(encodeHeartbeat());
    file.flush();
    file.close();

    ASSERT_TRUE(replay->loadFile(file.fileName()));
    QSignalSpy finishedSpy(replay, &telemetry::LogReplay::playbackFinished);
    QSignalSpy progressSpy(replay, &telemetry::LogReplay::progressChanged);

    replay->start(2.0);
    for (int i = 0; i < 50 && finishedSpy.count() == 0; ++i) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }

    EXPECT_GE(progressSpy.count(), 1);
    EXPECT_EQ(finishedSpy.count(), 1);
}
