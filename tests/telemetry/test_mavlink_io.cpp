#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QUdpSocket>
#include "telemetry/mavlink_io.hpp"
#include <mavlink.h>

using namespace aegis;

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

    bool connected = false;
    bool disconnected = false;
    int receivedCount = 0;

    QObject signalSink;
    QObject::connect(&io, &telemetry::MavlinkIO::connectionStateChanged, &signalSink,
                     [&](bool isConnected) {
                         if (isConnected) {
                             connected = true;
                         } else if (connected) {
                             disconnected = true;
                         }
                     });
    QObject::connect(&io, &telemetry::MavlinkIO::messageReceived, &signalSink,
                     [&](const telemetry::types::MavlinkMessage&) {
                         ++receivedCount;
                     });

    const quint16 port = 14650;
    io.start(QHostAddress::LocalHost, port);

    QUdpSocket sender;
    const QByteArray datagram = encodeHeartbeat();
    ASSERT_GT(sender.writeDatagram(datagram, QHostAddress::LocalHost, port), 0);

    EXPECT_TRUE(waitUntil([&]() { return connected && receivedCount > 0; }, 1000));
    EXPECT_TRUE(waitUntil([&]() { return disconnected; }, 1000));

    io.stop();
}
