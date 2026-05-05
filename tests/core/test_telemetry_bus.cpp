#include <gtest/gtest.h>
#include <QCoreApplication>
#include "core/bus/telemetry_bus.hpp"
#include "core/types/common.hpp"

using namespace aegis::core;

class TelemetryBusTest : public ::testing::Test {
protected:
    void SetUp() override {
        app = new QCoreApplication(argc, argv);
        bus = new TelemetryBus();
    }
    void TearDown() override {
        delete bus;
        delete app;
    }
    int argc = 1;
    char* argv[1] = {const_cast<char*>("test")};
    QCoreApplication* app{nullptr};
    TelemetryBus* bus{nullptr};
};

TEST_F(TelemetryBusTest, AttitudeSignalDelivery) {
    bool received = false;
    QObject sink;
    connect(bus, &TelemetryBus::attitudeChanged, &sink,
            [&received](const types::AttitudeData&) { received = true; });

    types::AttitudeData data;
    data.roll = 0.5;
    bus->emitAttitudeChanged(data);
    QCoreApplication::processEvents();

    EXPECT_TRUE(received);
}

TEST_F(TelemetryBusTest, TopicPubSub) {
    bool received = false;
    QObject sub;
    bus->subscribe("test.topic", &sub,
        [&received](const QVariant&) { received = true; });

    bus->publish("test.topic", QVariant(42));
    QCoreApplication::processEvents();

    EXPECT_TRUE(received);
}

TEST_F(TelemetryBusTest, OutboundCommandPosted) {
    bool received = false;
    QObject sink;
    connect(bus, &TelemetryBus::outboundCommand, &sink,
            [&received](const types::VehicleCommand& cmd) {
                received = (cmd.commandId == 16);
            });

    types::VehicleCommand cmd;
    cmd.commandId = 16;
    bus->postCommand(cmd);
    QCoreApplication::processEvents();

    EXPECT_TRUE(received);
}

TEST_F(TelemetryBusTest, CommandResponseSignalDelivery) {
    bool received = false;
    QObject sink;
    connect(bus, &TelemetryBus::commandResponse, &sink,
            [&received](quint16 commandId, bool success, const QString&) {
                received = success && commandId == 16;
            });

    bus->emitCommandResponse(16, true, QStringLiteral("ok"));
    QCoreApplication::processEvents();

    EXPECT_TRUE(received);
}
