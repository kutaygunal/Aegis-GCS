#include "map_view_plugin.hpp"
#include "core/bus/telemetry_bus.hpp"
#include "core/state/vehicle_state.hpp"
#include "ui/theme/theme_engine.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QPen>
#include <QBrush>
#include <QGraphicsPolygonItem>
#include <QPolygonF>
#include <QDebug>
#include <QtMath>

namespace aegis::plugins {

MapViewPlugin::MapViewPlugin(QObject* parent)
    : core::ITelemetrySink(parent) {}

MapViewPlugin::~MapViewPlugin() = default;

bool MapViewPlugin::initialize(core::TelemetryBus* bus,
                                core::VehicleState* state,
                                const QVariantMap& config) {
    Q_UNUSED(config)
    m_state = state;
    buildUi();
    setupScene();

    connect(bus, &core::TelemetryBus::positionChanged,
            this, &MapViewPlugin::onPositionChanged,
            Qt::QueuedConnection);
    connect(state, &core::VehicleState::missionCurrentChanged,
            this, &MapViewPlugin::onMissionCurrentChanged,
            Qt::QueuedConnection);

    return true;
}

QWidget* MapViewPlugin::widget() {
    return m_widget;
}

void MapViewPlugin::shutdown() {}

void MapViewPlugin::buildUi() {
    m_widget = new QWidget();
    m_widget->setStyleSheet(QStringLiteral("background-color: %1;")
                               .arg(aegis::ui::ThemeEngine::COLOR_BACKGROUND));

    auto* vbox = new QVBoxLayout(m_widget);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);

    // Toolbar
    auto* toolbar = new QHBoxLayout();
    auto* zoomInBtn = new QPushButton(tr("+"));
    auto* zoomOutBtn = new QPushButton(tr("-"));
    m_coordLabel = new QGraphicsTextItem();  // placeholder, we'll recreate in scene
    auto* coordText = new QLabel(tr("Lat: ---  Lon: ---"));
    coordText->setStyleSheet(QStringLiteral("color: %1; padding: 4px; font-family: monospace;")
                                .arg(aegis::ui::ThemeEngine::COLOR_TEXT));

    for (auto* btn : {zoomInBtn, zoomOutBtn}) {
        btn->setMaximumWidth(32);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: %1; color: %2; border: 1px solid %3; "
            "font-weight: bold; font-size: 14px; }"
            "QPushButton:hover { background-color: #37474F; }"
        ).arg(aegis::ui::ThemeEngine::COLOR_SURFACE,
             aegis::ui::ThemeEngine::COLOR_TEXT,
             aegis::ui::ThemeEngine::COLOR_TEXT));
    }

    connect(zoomInBtn, &QPushButton::clicked, this, &MapViewPlugin::zoomIn);
    connect(zoomOutBtn, &QPushButton::clicked, this, &MapViewPlugin::zoomOut);

    toolbar->addWidget(zoomInBtn);
    toolbar->addWidget(zoomOutBtn);
    toolbar->addStretch();
    toolbar->addWidget(coordText);
    vbox->addLayout(toolbar);

    // Graphics view
    m_view = new QGraphicsView();
    m_view->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setStyleSheet(QStringLiteral("background-color: %1; border: none;")
                              .arg(aegis::ui::ThemeEngine::COLOR_BACKGROUND));
    vbox->addWidget(m_view, 1);

    m_scene = new QGraphicsScene(this);
    m_view->setScene(m_scene);
}

void MapViewPlugin::setupScene() {
    // Coordinate grid: 10 major divisions, 100 minor
    qreal gridSize = 2000.0;
    qreal majorStep = gridSize / 10.0;

    // Background
    m_scene->addRect(-gridSize/2, -gridSize/2, gridSize, gridSize,
                     QPen(QColor(aegis::ui::ThemeEngine::COLOR_SURFACE)),
                     QBrush(QColor(aegis::ui::ThemeEngine::COLOR_BACKGROUND)));

    // Grid lines
    QPen minorPen(QColor("#333333"), 0.5);
    QPen majorPen(QColor("#555555"), 1.0);

    for (int i = -50; i <= 50; ++i) {
        qreal pos = i * (gridSize / 100.0);
        bool major = (i % 10 == 0);
        m_scene->addLine(pos, -gridSize/2, pos, gridSize/2, major ? majorPen : minorPen);
        m_scene->addLine(-gridSize/2, pos, gridSize/2, pos, major ? majorPen : minorPen);
    }

    // Crosshair at center (current map center)
    QPen crossPen(QColor(aegis::ui::ThemeEngine::COLOR_INFO), 1.5);
    crossPen.setStyle(Qt::DashLine);
    m_scene->addLine(-100, 0, 100, 0, crossPen);
    m_scene->addLine(0, -100, 0, 100, crossPen);

    // Vehicle marker (triangle pointing north by default)
    QPolygonF triangle;
    triangle << QPointF(0, -12) << QPointF(-8, 10) << QPointF(0, 6) << QPointF(8, 10);
    m_vehicleMarker = m_scene->addEllipse(-6, -6, 12, 12,
                                         QPen(QColor(aegis::ui::ThemeEngine::COLOR_GOOD), 2),
                                         QBrush(QColor(aegis::ui::ThemeEngine::COLOR_GOOD)));
    m_vehicleMarker->setZValue(10);

    // Track line
    m_trackLine = m_scene->addPath(QPainterPath(),
                                     QPen(QColor(aegis::ui::ThemeEngine::COLOR_INFO), 2));
    m_trackLine->setZValue(5);

    m_view->centerOn(0, 0);
    m_view->setSceneRect(-gridSize/2, -gridSize/2, gridSize, gridSize);
}

void MapViewPlugin::onPositionChanged(const core::types::PositionData& data) {
    qreal lat = data.lat / 1e7;
    qreal lon = data.lon / 1e7;
    qreal hdg = data.hdg / 100.0;

    // Update center on first fix
    if (m_trackHistory.isEmpty()) {
        m_centerLat = lat;
        m_centerLon = lon;
    }

    m_trackHistory.append(latLonToScene(lat, lon));
    if (m_trackHistory.size() > MAX_TRACK_POINTS) {
        m_trackHistory.removeFirst();
    }

    updateVehicleMarker(lat, lon, hdg);

    // Update track polyline
    QPainterPath path;
    if (!m_trackHistory.isEmpty()) {
        path.moveTo(m_trackHistory.first());
        for (int i = 1; i < m_trackHistory.size(); ++i) {
            path.lineTo(m_trackHistory[i]);
        }
    }
    m_trackLine->setPath(path);

    fitViewToVehicle();
}

void MapViewPlugin::onMissionCurrentChanged(int index) {
    Q_UNUSED(index)
    // TODO: highlight active waypoint on map
}

void MapViewPlugin::updateVehicleMarker(qreal lat, qreal lon, qreal hdg) {
    QPointF pos = latLonToScene(lat, lon);
    m_vehicleMarker->setPos(pos.x() - 6, pos.y() - 6);

    // Heading indicator as a line
    qreal rad = qDegreesToRadians(hdg);
    qreal dx = qSin(rad) * 20.0;
    qreal dy = -qCos(rad) * 20.0;

    // Remove old heading line if any and draw new
    static QGraphicsLineItem* hdgLine = nullptr;
    if (hdgLine) {
        m_scene->removeItem(hdgLine);
        delete hdgLine;
    }
    hdgLine = m_scene->addLine(pos.x(), pos.y(), pos.x() + dx, pos.y() + dy,
                                  QPen(QColor(aegis::ui::ThemeEngine::COLOR_WARNING), 2));
    hdgLine->setZValue(9);
}

QPointF MapViewPlugin::latLonToScene(qreal lat, qreal lon) const {
    qreal x = (lon - m_centerLon) * SCALE_FACTOR * qCos(qDegreesToRadians(m_centerLat));
    qreal y = -(lat - m_centerLat) * SCALE_FACTOR;  // negated: north is up
    return QPointF(x, y);
}

void MapViewPlugin::fitViewToVehicle() {
    if (!m_vehicleMarker) return;
    m_view->centerOn(m_vehicleMarker->pos().x() + 6, m_vehicleMarker->pos().y() + 6);
}

void MapViewPlugin::zoomIn() {
    m_scale *= 1.25;
    m_view->scale(1.25, 1.25);
}

void MapViewPlugin::zoomOut() {
    m_scale /= 1.25;
    m_view->scale(0.8, 0.8);
}

} // namespace aegis::plugins
