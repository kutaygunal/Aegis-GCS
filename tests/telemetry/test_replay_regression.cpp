#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QSignalSpy>
#include <QFile>
#include "core/bus/telemetry_bus.hpp"
#include "core/state/vehicle_state.hpp"
#include "telemetry/parsers.hpp"
#include "telemetry/replay/log_replay.hpp"

using namespace aegis;

class ReplayRegressionTest : public ::testing::Test {
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
    int argc = 1;
    char* argv[1] = {const_cast<char*>("test")};
    QCoreApplication* app{nullptr};
    core::TelemetryBus* bus{nullptr};
    core::VehicleState* state{nullptr};
    telemetry::MavlinkParser* parser{nullptr};
    telemetry::LogReplay* replay{nullptr};

    QString corpusPath() const {
        return QStringLiteral(REPLAY_CORPUS_PATH);
    }
};

TEST_F(ReplayRegressionTest, CorpusFileExistsAndNonEmpty) {
    QFile file(corpusPath());
    EXPECT_TRUE(file.exists());
    EXPECT_GT(file.size(), 0);
}

TEST_F(ReplayRegressionTest, AllMessagesReplayed) {
    ASSERT_TRUE(replay->loadFile(corpusPath()));
    int messageCount = 0;
    while (replay->progress() < 1.0) {
        replay->step();
        QCoreApplication::processEvents();
        ++messageCount;
        if (messageCount > 50) break; // safety limit
    }
    EXPECT_EQ(messageCount, 7);
    EXPECT_EQ(replay->progress(), 1.0);
}

TEST_F(ReplayRegressionTest, HeartbeatFinalStateIsStandbyDisarmed) {
    ASSERT_TRUE(replay->loadFile(corpusPath()));
    while (replay->progress() < 1.0) {
        replay->step();
        QCoreApplication::processEvents();
    }

    EXPECT_FALSE(state->systemState().armed);
    EXPECT_EQ(state->systemState().status, core::types::SystemStatus::Standby);
}

TEST_F(ReplayRegressionTest, AttitudeParsed) {
    ASSERT_TRUE(replay->loadFile(corpusPath()));
    while (replay->progress() < 1.0) {
        replay->step();
        QCoreApplication::processEvents();
    }

    EXPECT_NEAR(state->attitude().roll, 0.1, 0.001);
    EXPECT_NEAR(state->attitude().pitch, 0.2, 0.001);
    EXPECT_NEAR(state->attitude().yaw, 0.3, 0.001);
}

TEST_F(ReplayRegressionTest, PositionParsed) {
    ASSERT_TRUE(replay->loadFile(corpusPath()));
    while (replay->progress() < 1.0) {
        replay->step();
        QCoreApplication::processEvents();
    }

    EXPECT_EQ(state->position().lat, 377000000);
    EXPECT_EQ(state->position().lon, -1220000000);
}

TEST_F(ReplayRegressionTest, BatteryParsed) {
    ASSERT_TRUE(replay->loadFile(corpusPath()));
    while (replay->progress() < 1.0) {
        replay->step();
        QCoreApplication::processEvents();
    }

    EXPECT_EQ(state->battery().remaining, 85);
    EXPECT_NEAR(state->battery().voltageV, 15.2, 0.1);
}

TEST_F(ReplayRegressionTest, MissionCurrentParsed) {
    ASSERT_TRUE(replay->loadFile(corpusPath()));
    while (replay->progress() < 1.0) {
        replay->step();
        QCoreApplication::processEvents();
    }

    EXPECT_EQ(state->missionCurrent(), 3);
}

TEST_F(ReplayRegressionTest, StatusTextParsed) {
    QSignalSpy alertSpy(bus, &core::TelemetryBus::alertRaised);
    ASSERT_TRUE(replay->loadFile(corpusPath()));
    while (replay->progress() < 1.0) {
        replay->step();
        QCoreApplication::processEvents();
    }

    bool foundBatteryAlert = false;
    for (int i = 0; i < alertSpy.count(); ++i) {
        QList<QVariant> args = alertSpy.takeAt(i);
        QString message = args[1].toString();
        if (message.contains("Low battery", Qt::CaseInsensitive)) {
            foundBatteryAlert = true;
            break;
        }
    }
    EXPECT_TRUE(foundBatteryAlert);
}
