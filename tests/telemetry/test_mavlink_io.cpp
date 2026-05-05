#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QUdpSocket>
#include "telemetry/mavlink_io.hpp"
#include "core/types/common.hpp"
#include <mavlink.h>

using namespace aegis;
using BusState = aegis::core::types::ConnectionState;

class MavlinkIOTest : public ::testing::Test {
protected:
    void SetUp() override {
        app = new QCoreApplication(argc, argv);
    }

    void TearDown() override {
        delete app;
    }

    bool waitUntil(const std::function<bool()>& predicate, int timeoutMs = 1000) {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < timeoutMs) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            if (predicate()) return true;
        }
        return predicate();
    }

    QByteArray encodeHeartbeat() {
        mavlink_message_t msg{};
        mavlink_msg_heartbeat_pack(1, 1, &msg,
                                   MAV_TYPE_QUADROTOR,
                                   MAV_AUTOPILOT_PX4,
                                   MAV_MODE_FLAG_MANUAL_INPUT_ENABLED,
                                   0,
                                   MAV_STATE_ACTIVE);
        uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
        const auto len = mavlink_msg_to_send_buffer(buffer, &msg);
        return QByteArray(reinterpret_cast<const char*>(buffer), len);
    }

    int argc = 1;
    char* argv[1] = {const_cast<char*>("test")};
    QCoreApplication* app{nullptr};
};

TEST_F(MavlinkIOTest, EmitsConnectedAfterHeartbeatAndDisconnectedOnTimeout) {
    telemetry::MavlinkIO io;
    io.setHeartbeatTimeoutMs(100);

    BusState observedState = BusState::Disconnected;
    int receivedCount = 0;

    QObject signalSink;
    QObject::connect(&io, &telemetry::MavlinkIO::connectionStateChanged, &signalSink,
                     [&observedState](BusState state) { observedState = state; });
    QObject::connect(&io, &telemetry::MavlinkIO::messageReceived, &signalSink,
                     [&receivedCount](const telemetry::types::MavlinkMessage&) {
                         ++receivedCount;
                     });

    const quint16 port = 14650;
    io.start(QHostAddress::LocalHost, port);

    // Wait for socket to bind
    EXPECT_TRUE(waitUntil([&io]() {
        return io.connectionState() == telemetry::ConnectionStateMachine::State::SocketBound;
    }, 500));

    // Send heartbeat
    QUdpSocket sender;
    const QByteArray datagram = encodeHeartbeat();
    ASSERT_GT(sender.writeDatagram(datagram, QHostAddress::LocalHost, port), 0);

    // Wait for heartbeat to be processed -> HeartbeatAlive
    EXPECT_TRUE(waitUntil([&observedState]() {
        return observedState == BusState::HeartbeatAlive;
    }, 1000));
    EXPECT_TRUE(waitUntil([&receivedCount]() { return receivedCount > 0; }, 1000));

    // Wait for timeout to trigger Degraded then Reconnecting
    // With 100ms timeout: first timeout -> Degraded, second timeout -> Reconnecting
    EXPECT_TRUE(waitUntil([&observedState]() {
        return observedState == BusState::Reconnecting;
    }, 1000));

    io.stop();
}

TEST_F(MavlinkIOTest, EmitsSocketBoundOnStart) {
    telemetry::MavlinkIO io;

    BusState observedState = BusState::Disconnected;
    QObject signalSink;
    QObject::connect(&io, &telemetry::MavlinkIO::connectionStateChanged, &signalSink,
                     [&observedState](BusState state) { observedState = state; });

    const quint16 port = 14651;
    io.start(QHostAddress::LocalHost, port);

    EXPECT_TRUE(waitUntil([&observedState]() {
        return observedState == BusState::SocketBound;
    }, 500));

    EXPECT_EQ(io.connectionState(), telemetry::ConnectionStateMachine::State::SocketBound);

    io.stop();
}

TEST_F(MavlinkIOTest, EmitsVehicleDiscoveredOnFirstPacket) {
    telemetry::MavlinkIO io;
    io.setHeartbeatTimeoutMs(500);

    BusState observedState = BusState::Disconnected;
    QObject signalSink;
    QObject::connect(&io, &telemetry::MavlinkIO::connectionStateChanged, &signalSink,
                     [&observedState](BusState state) { observedState = state; });

    const quint16 port = 14652;
    io.start(QHostAddress::LocalHost, port);

    // Wait for socket bound
    EXPECT_TRUE(waitUntil([&observedState]() {
        return observedState == BusState::SocketBound;
    }, 500));

    // Send a non-heartbeat packet (e.g., ATTITUDE)
    QUdpSocket sender;
    mavlink_message_t msg{};
    mavlink_msg_attitude_pack(1, 1, &msg, 0, 0.1f, 0.2f, 0.3f, 0.0f, 0.0f, 0.0f);
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    const auto len = mavlink_msg_to_send_buffer(buffer, &msg);
    QByteArray datagram(reinterpret_cast<const char*>(buffer), len);
    ASSERT_GT(sender.writeDatagram(datagram, QHostAddress::LocalHost, port), 0);

    // Should transition to VehicleDiscovered
    EXPECT_TRUE(waitUntil([&observedState]() {
        return observedState == BusState::VehicleDiscovered;
    }, 1000));

    io.stop();
}
