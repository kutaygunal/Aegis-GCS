#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QObject>
#include "telemetry/connection_state_machine.hpp"

using namespace aegis::telemetry;
using State = ConnectionStateMachine::State;

class ConnectionStateMachineTest : public ::testing::Test {
protected:
    void SetUp() override {
        app = new QCoreApplication(argc, argv);
        machine = new ConnectionStateMachine();
    }

    void TearDown() override {
        delete machine;
        delete app;
    }

    int argc = 1;
    char* argv[1] = {const_cast<char*>("test")};
    QCoreApplication* app{nullptr};
    ConnectionStateMachine* machine{nullptr};
};

TEST_F(ConnectionStateMachineTest, InitialStateIsDisconnected) {
    EXPECT_EQ(machine->currentState(), State::Disconnected);
}

TEST_F(ConnectionStateMachineTest, NormalSequence) {
    std::vector<State> stateHistory;
    std::vector<std::tuple<State, State, QString>> transitionHistory;

    QObject observer;
    QObject::connect(machine, &ConnectionStateMachine::stateChanged, &observer,
        [&stateHistory](State s) { stateHistory.push_back(s); });
    QObject::connect(machine, &ConnectionStateMachine::stateTransitioned, &observer,
        [&transitionHistory](State oldS, State newS, const QString& reason) {
            transitionHistory.emplace_back(oldS, newS, reason);
        });

    machine->onSocketBound();
    EXPECT_EQ(machine->currentState(), State::SocketBound);

    machine->onPacketReceived();
    EXPECT_EQ(machine->currentState(), State::VehicleDiscovered);

    machine->onHeartbeatReceived();
    EXPECT_EQ(machine->currentState(), State::HeartbeatAlive);

    ASSERT_EQ(transitionHistory.size(), 3u);
    EXPECT_EQ(std::get<0>(transitionHistory[0]), State::Disconnected);
    EXPECT_EQ(std::get<1>(transitionHistory[0]), State::SocketBound);

    EXPECT_EQ(std::get<0>(transitionHistory[1]), State::SocketBound);
    EXPECT_EQ(std::get<1>(transitionHistory[1]), State::VehicleDiscovered);

    EXPECT_EQ(std::get<0>(transitionHistory[2]), State::VehicleDiscovered);
    EXPECT_EQ(std::get<1>(transitionHistory[2]), State::HeartbeatAlive);
}

TEST_F(ConnectionStateMachineTest, TimeoutToDegraded) {
    machine->onSocketBound();
    machine->onPacketReceived();
    machine->onHeartbeatReceived();
    EXPECT_EQ(machine->currentState(), State::HeartbeatAlive);

    machine->onHeartbeatTimeout();
    EXPECT_EQ(machine->currentState(), State::Degraded);
}

TEST_F(ConnectionStateMachineTest, TimeoutToReconnecting) {
    machine->onSocketBound();
    machine->onPacketReceived();
    machine->onHeartbeatReceived();

    machine->onHeartbeatTimeout();  // HeartbeatAlive -> Degraded
    machine->onHeartbeatTimeout();  // Degraded -> Reconnecting
    EXPECT_EQ(machine->currentState(), State::Reconnecting);
}

TEST_F(ConnectionStateMachineTest, RecoverFromDegraded) {
    machine->onSocketBound();
    machine->onPacketReceived();
    machine->onHeartbeatReceived();
    machine->onHeartbeatTimeout();
    EXPECT_EQ(machine->currentState(), State::Degraded);

    machine->onHeartbeatReceived();
    EXPECT_EQ(machine->currentState(), State::HeartbeatAlive);
}

TEST_F(ConnectionStateMachineTest, RecoverFromReconnecting) {
    machine->onSocketBound();
    machine->onPacketReceived();
    machine->onHeartbeatReceived();
    machine->onHeartbeatTimeout();
    machine->onHeartbeatTimeout();
    EXPECT_EQ(machine->currentState(), State::Reconnecting);

    machine->onHeartbeatReceived();
    EXPECT_EQ(machine->currentState(), State::HeartbeatAlive);
}

TEST_F(ConnectionStateMachineTest, StopFromAnyState) {
    // From SocketBound
    machine->onSocketBound();
    machine->onStop();
    EXPECT_EQ(machine->currentState(), State::Disconnected);

    // From HeartbeatAlive
    machine->onSocketBound();
    machine->onPacketReceived();
    machine->onHeartbeatReceived();
    machine->onStop();
    EXPECT_EQ(machine->currentState(), State::Disconnected);

    // From Reconnecting
    machine->onSocketBound();
    machine->onPacketReceived();
    machine->onHeartbeatReceived();
    machine->onHeartbeatTimeout();
    machine->onHeartbeatTimeout();
    EXPECT_EQ(machine->currentState(), State::Reconnecting);
    machine->onStop();
    EXPECT_EQ(machine->currentState(), State::Disconnected);
}

TEST_F(ConnectionStateMachineTest, HeartbeatWhileDisconnectedDoesNothing) {
    machine->onHeartbeatReceived();
    EXPECT_EQ(machine->currentState(), State::Disconnected);
}

TEST_F(ConnectionStateMachineTest, DoubleTimeoutFromHeartbeatAlive) {
    machine->onSocketBound();
    machine->onPacketReceived();
    machine->onHeartbeatReceived();

    machine->onHeartbeatTimeout();  // -> Degraded
    machine->onHeartbeatTimeout();  // -> Reconnecting
    machine->onHeartbeatTimeout();  // Stay Reconnecting

    EXPECT_EQ(machine->currentState(), State::Reconnecting);
}

TEST_F(ConnectionStateMachineTest, StateToStringCoverage) {
    EXPECT_EQ(ConnectionStateMachine::stateToString(State::Disconnected),      QString("Disconnected"));
    EXPECT_EQ(ConnectionStateMachine::stateToString(State::SocketBound),       QString("SocketBound"));
    EXPECT_EQ(ConnectionStateMachine::stateToString(State::VehicleDiscovered), QString("VehicleDiscovered"));
    EXPECT_EQ(ConnectionStateMachine::stateToString(State::HeartbeatAlive),    QString("HeartbeatAlive"));
    EXPECT_EQ(ConnectionStateMachine::stateToString(State::Degraded),         QString("Degraded"));
    EXPECT_EQ(ConnectionStateMachine::stateToString(State::Reconnecting),      QString("Reconnecting"));
}

TEST_F(ConnectionStateMachineTest, StateToConnectionStateMapping) {
    using BusState = aegis::core::types::ConnectionState;
    EXPECT_EQ(ConnectionStateMachine::stateToConnectionState(State::Disconnected),      BusState::Disconnected);
    EXPECT_EQ(ConnectionStateMachine::stateToConnectionState(State::SocketBound),       BusState::SocketBound);
    EXPECT_EQ(ConnectionStateMachine::stateToConnectionState(State::VehicleDiscovered), BusState::VehicleDiscovered);
    EXPECT_EQ(ConnectionStateMachine::stateToConnectionState(State::HeartbeatAlive),    BusState::HeartbeatAlive);
    EXPECT_EQ(ConnectionStateMachine::stateToConnectionState(State::Degraded),         BusState::Degraded);
    EXPECT_EQ(ConnectionStateMachine::stateToConnectionState(State::Reconnecting),      BusState::Reconnecting);
}

TEST_F(ConnectionStateMachineTest, NoDuplicateTransitions) {
    std::vector<std::tuple<State, State, QString>> transitionHistory;

    QObject observer;
    QObject::connect(machine, &ConnectionStateMachine::stateTransitioned, &observer,
        [&transitionHistory](State oldS, State newS, const QString& r) {
            transitionHistory.emplace_back(oldS, newS, r);
        });

    machine->onSocketBound();
    machine->onSocketBound();  // Second call should not re-emit
    EXPECT_EQ(transitionHistory.size(), 1u);
}
