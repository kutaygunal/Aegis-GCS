#include <gtest/gtest.h>
#include <QApplication>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QSignalSpy>
#include "plugins/map_view/follow_vehicle_controller.hpp"

using namespace aegis::plugins;

class FollowVehicleTest : public ::testing::Test {
protected:
    static std::unique_ptr<QApplication> app;
    QGraphicsView* view = nullptr;
    FollowVehicleController* ctrl = nullptr;

    void SetUp() override {
        if (!QCoreApplication::instance()) {
            static int argc = 1;
            static char* argv[] = {const_cast<char*>("test")};
            app = std::make_unique<QApplication>(argc, argv);
        }
        view = new QGraphicsView();
        view->setScene(new QGraphicsScene());
        view->setGeometry(0, 0, 512, 512);
        view->scene()->setSceneRect(0, 0, 10000, 10000);
        view->show(); // needed for viewport geometry
        view->centerOn(5000, 5000);

        ctrl = new FollowVehicleController(view);
    }

    void TearDown() override {
        delete ctrl;
        delete view;
    }

    void processEvents(int ms = 50) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, ms);
    }
};

std::unique_ptr<QApplication> FollowVehicleTest::app;

TEST_F(FollowVehicleTest, DefaultsToActive) {
    EXPECT_EQ(ctrl->state(), FollowState::Active);
    EXPECT_TRUE(ctrl->isActive());
}

TEST_F(FollowVehicleTest, ActiveToSuspendedOnMissingTiles) {
    QSignalSpy spy(ctrl, &FollowVehicleController::stateChanged);
    ctrl->setViewportFullyLoaded(false);
    processEvents();

    EXPECT_EQ(ctrl->state(), FollowState::Suspended);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(qvariant_cast<FollowState>(spy.first().first()), FollowState::Suspended);
}

TEST_F(FollowVehicleTest, SuspendedToActiveOnTilesLoaded) {
    ctrl->setViewportFullyLoaded(false);
    processEvents();
    EXPECT_EQ(ctrl->state(), FollowState::Suspended);

    QSignalSpy spy(ctrl, &FollowVehicleController::stateChanged);
    ctrl->setViewportFullyLoaded(true);
    processEvents();

    EXPECT_EQ(ctrl->state(), FollowState::Active);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(qvariant_cast<FollowState>(spy.first().first()), FollowState::Active);
}

TEST_F(FollowVehicleTest, ActiveToDisabledOnUserPan) {
    QSignalSpy spy(ctrl, &FollowVehicleController::stateChanged);
    ctrl->onUserPanned();
    processEvents();

    EXPECT_EQ(ctrl->state(), FollowState::Disabled);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(FollowVehicleTest, DisabledToActiveOnRecenterWhenTilesReady) {
    ctrl->onUserPanned();
    processEvents();
    EXPECT_EQ(ctrl->state(), FollowState::Disabled);

    ctrl->setViewportFullyLoaded(true);
    QSignalSpy spy(ctrl, &FollowVehicleController::stateChanged);
    ctrl->requestRecenter();
    processEvents();

    EXPECT_EQ(ctrl->state(), FollowState::Active);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(FollowVehicleTest, DisabledToSuspendedOnRecenterWhenTilesMissing) {
    ctrl->onUserPanned();
    processEvents();
    EXPECT_EQ(ctrl->state(), FollowState::Disabled);

    ctrl->setViewportFullyLoaded(false);
    QSignalSpy spy(ctrl, &FollowVehicleController::stateChanged);
    ctrl->requestRecenter();
    processEvents();

    EXPECT_EQ(ctrl->state(), FollowState::Suspended);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(FollowVehicleTest, NoDuplicateStateTransitions) {
    ctrl->setViewportFullyLoaded(false);
    QSignalSpy spy(ctrl, &FollowVehicleController::stateChanged);
    ctrl->setViewportFullyLoaded(false); // already suspended
    ctrl->setViewportFullyLoaded(false);
    processEvents();

    EXPECT_EQ(spy.count(), 0); // no additional signals
    EXPECT_EQ(ctrl->state(), FollowState::Suspended);
}

TEST_F(FollowVehicleTest, InterpolationConverges) {
    ctrl->setViewportFullyLoaded(true);
    QPointF start = view->mapToScene(view->viewport()->rect().center());
    QPointF target(6000, 6000);

    ctrl->setTarget(target);

    // Tick manually enough times for lerp to get close
    for (int i = 0; i < 500; ++i) {
        QMetaObject::invokeMethod(ctrl, "onPanTick", Qt::DirectConnection);
    }

    QPointF end = view->mapToScene(view->viewport()->rect().center());

    // Should have moved significantly toward target (more than half way)
    qreal startDist = QLineF(start, target).length();
    qreal endDist = QLineF(end, target).length();
    EXPECT_LT(endDist, startDist * 0.5);

    // Should be within a few pixels of target after 500 ticks
    EXPECT_NEAR(end.x(), target.x(), 20.0);
    EXPECT_NEAR(end.y(), target.y(), 20.0);
}

TEST_F(FollowVehicleTest, TargetIgnoredWhenSuspended) {
    ctrl->setViewportFullyLoaded(false);
    processEvents();
    EXPECT_EQ(ctrl->state(), FollowState::Suspended);

    QSignalSpy finishedSpy(ctrl, &FollowVehicleController::smoothPanFinished);
    ctrl->setTarget(QPointF(9999, 9999));
    processEvents(100);

    EXPECT_EQ(finishedSpy.count(), 0);
}

TEST_F(FollowVehicleTest, TargetIgnoredWhenDisabled) {
    ctrl->onUserPanned();
    processEvents();
    EXPECT_EQ(ctrl->state(), FollowState::Disabled);

    QSignalSpy finishedSpy(ctrl, &FollowVehicleController::smoothPanFinished);
    ctrl->setTarget(QPointF(9999, 9999));
    processEvents(100);

    EXPECT_EQ(finishedSpy.count(), 0);
}

TEST_F(FollowVehicleTest, MultipleRapidTransitions) {
    // Rapid tile flapping should not crash or emit excessive signals
    QSignalSpy spy(ctrl, &FollowVehicleController::stateChanged);

    ctrl->setViewportFullyLoaded(false); // -> Suspended
    ctrl->setViewportFullyLoaded(true);  // -> Active
    ctrl->setViewportFullyLoaded(false); // -> Suspended
    ctrl->onUserPanned();                // -> Disabled
    ctrl->setViewportFullyLoaded(true);  // stays Disabled
    ctrl->requestRecenter();             // -> Active
    processEvents();

    EXPECT_EQ(ctrl->state(), FollowState::Active);
    EXPECT_EQ(spy.count(), 5); // each actual transition
}
