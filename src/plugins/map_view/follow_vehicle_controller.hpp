#pragma once

#include <QObject>
#include <QPointF>
#include <QTimer>

QT_BEGIN_NAMESPACE
class QGraphicsView;
QT_END_NAMESPACE

namespace aegis::plugins {

/**
 * @brief Follow-vehicle state machine with smooth interpolated panning.
 *
 * States:
 *   Active    — smoothly centers the view on the vehicle target.
 *   Suspended — vehicle is moving but tiles are missing; holds last good center.
 *   Disabled  — user manually panned; waits for explicit re-center request.
 */
enum class FollowState {
    Active,
    Suspended,
    Disabled
};

class FollowVehicleController : public QObject {
    Q_OBJECT
public:
    explicit FollowVehicleController(QGraphicsView* view, QObject* parent = nullptr);
    ~FollowVehicleController() override;

    void setTarget(const QPointF& scenePos);
    void setViewportFullyLoaded(bool loaded);
    void onUserPanned();
    void requestRecenter();

    FollowState state() const { return m_state; }
    bool isActive() const { return m_state == FollowState::Active; }
    bool isSuspended() const { return m_state == FollowState::Suspended; }
    bool isDisabled() const { return m_state == FollowState::Disabled; }

signals:
    void stateChanged(FollowState state);
    void smoothPanFinished();

private slots:
    void onPanTick();

private:
    void transitionTo(FollowState newState);
    void startPan();
    void stopPan();

    QGraphicsView* m_view;
    FollowState m_state = FollowState::Active;
    QPointF m_target;
    bool m_viewportLoaded = true;
    QTimer* m_panTimer;

    static constexpr int PAN_INTERVAL_MS = 16;   // ~60fps
    static constexpr qreal PAN_LERP = 0.12;      // interpolation factor
    static constexpr qreal PAN_TOLERANCE = 0.5;  // px distance to snap
};

} // namespace aegis::plugins
