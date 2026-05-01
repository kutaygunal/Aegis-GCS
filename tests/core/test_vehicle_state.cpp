#include <gtest/gtest.h>
#include <QCoreApplication>
#include "core/state/vehicle_state.hpp"

using namespace aegis::core;

class VehicleStateTest : public ::testing::Test {
protected:
    void SetUp() override {
        app = new QCoreApplication(argc, argv);
        state = new VehicleState();
    }
    void TearDown() override {
        delete state;
        delete app;
    }
    int argc = 1;
    char* argv[1] = {const_cast<char*>("test")};
    QCoreApplication* app{nullptr};
    VehicleState* state{nullptr};
};

TEST_F(VehicleStateTest, AttitudeUpdate) {
    types::AttitudeData data;
    data.roll = 0.78;
    data.pitch = -0.12;
    data.yaw = 1.57;

    state->updateAttitude(data);
    auto result = state->attitude();

    EXPECT_DOUBLE_EQ(result.roll, 0.78);
    EXPECT_DOUBLE_EQ(result.pitch, -0.12);
    EXPECT_DOUBLE_EQ(result.yaw, 1.57);
}

TEST_F(VehicleStateTest, MissionCurrentUpdate) {
    state->updateMissionCurrent(3);
    EXPECT_EQ(state->missionCurrent(), 3);
}
