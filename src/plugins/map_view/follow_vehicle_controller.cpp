#include "follow_vehicle_controller.hpp"
#include <QGraphicsView>
#include <QMouseEvent>

namespace aegis::plugins {

FollowVehicleController::FollowVehicleController(QGraphicsView* view, QObject* parent)
    : QObject(parent)
    , m_view(view)
    , m_panTimer(new QTimer(this)) {
    m_panTimer->setInterval(PAN_INTERVAL_MS);
    connect(m_panTimer, &QTimer::timeout, this, &FollowVehicleController::onPanTick);
}

FollowVehicleController::~FollowVehicleController() = default;

void FollowVehicleController::setTarget(const QPointF& scenePos) {
    m_target = scenePos;
    if (m_state == FollowState::Active && !m_panTimer->isActive()) {
        startPan();
    }
}

void FollowVehicleController::setViewportFullyLoaded(bool loaded) {
    if (m_viewportLoaded == loaded) return;
    m_viewportLoaded = loaded;

    if (loaded) {
        // Tiles now ready: resume if we were suspended, or allow recenter
        if (m_state == FollowState::Suspended) {
            transitionTo(FollowState::Active);
            startPan();
        }
    } else {
        // Tiles missing: suspend if active
        if (m_state == FollowState::Active) {
            transitionTo(FollowState::Suspended);
            stopPan();
        }
    }
}

void FollowVehicleController::onUserPanned() {
    if (m_state == FollowState::Disabled) return;
    transitionTo(FollowState::Disabled);
    stopPan();
}

void FollowVehicleController::requestRecenter() {
    if (m_viewportLoaded) {
        transitionTo(FollowState::Active);
        startPan();
    } else {
        transitionTo(FollowState::Suspended);
    }
}

void FollowVehicleController::transitionTo(FollowState newState) {
    if (m_state == newState) return;
    m_state = newState;
    emit stateChanged(m_state);
}

void FollowVehicleController::startPan() {
    if (!m_panTimer->isActive()) {
        m_panTimer->start();
    }
}

void FollowVehicleController::stopPan() {
    m_panTimer->stop();
}

void FollowVehicleController::onPanTick() {
    if (m_state != FollowState::Active || !m_view) {
        stopPan();
        return;
    }

    QRect vp = m_view->viewport()->rect();
    QPointF current = m_view->mapToScene(vp.center());
    QPointF diff = m_target - current;

    if (qAbs(diff.x()) < PAN_TOLERANCE && qAbs(diff.y()) < PAN_TOLERANCE) {
        m_view->centerOn(m_target);
        emit smoothPanFinished();
        stopPan();
        return;
    }

    QPointF next = current + diff * PAN_LERP;
    m_view->centerOn(next);
}

} // namespace aegis::plugins
