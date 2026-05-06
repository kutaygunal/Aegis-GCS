#include "map_view_plugin.hpp"
#include "map_math.hpp"
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
#include <QPixmap>
#include <QPolygonF>
#include <QDebug>
#include <QtMath>
#include <QJsonObject>
#include <QMouseEvent>

namespace aegis::plugins {

MapViewPlugin::MapViewPlugin(QObject* parent)
    : core::ITelemetrySink(parent) {}

MapViewPlugin::~MapViewPlugin() = default;

bool MapViewPlugin::initialize(core::TelemetryBus* bus,
                                core::VehicleState* state,
                                const QVariantMap& config) {
    m_zoom = qBound(1, config.value("zoom", 15).toInt(), 19);
    m_tileUrlTemplate = config.value(
        "tileUrlTemplate",
        QStringLiteral("https://tile.openstreetmap.org/%1/%2/%3.png")).toString();
    m_state = state;
    m_bus = bus;
    buildUi();
    setupScene();

    // TileLoader
    m_tileLoader = new aegis::map::TileLoader(this);
    QJsonObject tileConfig;
    tileConfig["tileUrlTemplate"] = m_tileUrlTemplate;
    tileConfig["maxConcurrentDownloads"] = config.value("maxConcurrentDownloads", 6).toInt();
    tileConfig["maxRetries"] = config.value("maxRetries", 2).toInt();
    tileConfig["tileCacheSizeMB"] = config.value("tileCacheSizeMB", 256).toInt();
    m_tileLoader->setConfig(tileConfig);

    connect(m_tileLoader, &aegis::map::TileLoader::tileReady,
            this, &MapViewPlugin::onTileReady);
    connect(m_tileLoader, &aegis::map::TileLoader::tileFailed,
            this, &MapViewPlugin::onTileFailed);

    // Follow-vehicle controller
    m_followController = new FollowVehicleController(m_view, this);
    connect(m_followController, &FollowVehicleController::stateChanged,
            this, &MapViewPlugin::onFollowStateChanged);

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

bool MapViewPlugin::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_view->viewport()) {
        if (event->type() == QEvent::MouseMove) {
            auto* me = reinterpret_cast<QMouseEvent*>(event);
            if (me->buttons() & Qt::LeftButton) {
                m_followController->onUserPanned();
            }
        }
    }
    return QObject::eventFilter(obj, event);
}

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
    m_coordLabel = new QLabel(tr("Lat: ---  Lon: ---"));
    m_coordLabel->setStyleSheet(QStringLiteral("color: %1; padding: 4px; font-family: monospace;")
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

    // Re-center button (hidden by default)
    m_recenterBtn = new QPushButton(tr("Re-center"));
    m_recenterBtn->setVisible(false);
    m_recenterBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: %1; color: %2; border: 1px solid %3; "
        "padding: 4px 12px; font-size: 11px; }"
        "QPushButton:hover { background-color: #37474F; }"
    ).arg(aegis::ui::ThemeEngine::COLOR_SURFACE,
         aegis::ui::ThemeEngine::COLOR_TEXT,
         aegis::ui::ThemeEngine::COLOR_TEXT));
    connect(m_recenterBtn, &QPushButton::clicked,
            this, &MapViewPlugin::onRecenterClicked);
    toolbar->addWidget(m_recenterBtn);

    // Status label for suspended indicator
    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet(QStringLiteral(
        "color: %1; padding: 4px; font-size: 11px; font-weight: bold;"
    ).arg(aegis::ui::ThemeEngine::COLOR_WARNING));
    toolbar->addWidget(m_statusLabel);

    toolbar->addWidget(m_coordLabel);
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

    // Detect manual pan
    m_view->viewport()->installEventFilter(this);
}

void MapViewPlugin::setupScene() {
    const qreal sceneExtent = TILE_SIZE * (TILE_RADIUS * 2 + 3);
    m_scene->setSceneRect(-sceneExtent / 2, -sceneExtent / 2, sceneExtent, sceneExtent);
    m_scene->setBackgroundBrush(QBrush(QColor(aegis::ui::ThemeEngine::COLOR_BACKGROUND)));

    // Crosshair at center map reference.
    QPen crossPen(QColor(aegis::ui::ThemeEngine::COLOR_INFO), 1.5);
    crossPen.setStyle(Qt::DashLine);
    auto* h = m_scene->addLine(-100, 0, 100, 0, crossPen);
    auto* v = m_scene->addLine(0, -100, 0, 100, crossPen);
    h->setZValue(2);
    v->setZValue(2);

    m_vehicleMarker = m_scene->addEllipse(-6, -6, 12, 12,
                                         QPen(QColor(aegis::ui::ThemeEngine::COLOR_GOOD), 2),
                                         QBrush(QColor(aegis::ui::ThemeEngine::COLOR_GOOD)));
    m_vehicleMarker->setZValue(20);

    m_trackLine = m_scene->addPath(QPainterPath(),
                                     QPen(QColor(aegis::ui::ThemeEngine::COLOR_INFO), 2));
    m_trackLine->setZValue(10);

    updateTiles();
    m_view->centerOn(0, 0);
}

void MapViewPlugin::onPositionChanged(const core::types::PositionData& data) {
    qreal lat = data.lat / 1e7;
    qreal lon = data.lon / 1e7;
    qreal hdg = data.hdg / 100.0;

    m_lastLat = lat;
    m_lastLon = lon;
    m_lastHeading = hdg;

    // Update center on first fix.
    if (!m_hasFix) {
        m_centerLat = lat;
        m_centerLon = lon;
        m_hasFix = true;
    }

    m_trackHistoryLatLon.append(qMakePair(lat, lon));
    if (m_trackHistoryLatLon.size() > MAX_TRACK_POINTS) {
        m_trackHistoryLatLon.removeFirst();
    }

    updateVehicleMarker(lat, lon, hdg);
    updateVisibleTiles();

    // Update track polyline
    QPainterPath path;
    if (!m_trackHistoryLatLon.isEmpty()) {
        path.moveTo(latLonToScene(m_trackHistoryLatLon.first().first,
                                   m_trackHistoryLatLon.first().second));
        for (int i = 1; i < m_trackHistoryLatLon.size(); ++i) {
            path.lineTo(latLonToScene(m_trackHistoryLatLon[i].first,
                                       m_trackHistoryLatLon[i].second));
        }
    }
    m_trackLine->setPath(path);

    // Feed target to follow-vehicle controller (it decides whether to pan)
    if (m_followController) {
        m_followController->setTarget(latLonToScene(lat, lon));
    }

    QString coordText = QStringLiteral("Lat: %1  Lon: %2")
                            .arg(lat, 0, 'f', 6)
                            .arg(lon, 0, 'f', 6);
    if (m_coordLabel) m_coordLabel->setText(coordText);
}

void MapViewPlugin::onMissionCurrentChanged(int index) {
    Q_UNUSED(index)
    // TODO: highlight active waypoint on map
}

void MapViewPlugin::updateVehicleMarker(qreal lat, qreal lon, qreal hdg) {
    QPointF pos = latLonToScene(lat, lon);
    m_vehicleMarker->setPos(pos);

    // Heading indicator as a line
    qreal rad = qDegreesToRadians(hdg);
    qreal dx = qSin(rad) * 20.0;
    qreal dy = -qCos(rad) * 20.0;

    // Remove old heading line if any and draw new
    if (m_headingLine) {
        m_scene->removeItem(m_headingLine);
        delete m_headingLine;
    }
    m_headingLine = m_scene->addLine(pos.x(), pos.y(), pos.x() + dx, pos.y() + dy,
                                  QPen(QColor(aegis::ui::ThemeEngine::COLOR_WARNING), 2));
    m_headingLine->setZValue(9);
}

QPointF MapViewPlugin::latLonToScene(qreal lat, qreal lon) const {
    return latLonToWorldPixel(lat, lon) - latLonToWorldPixel(m_centerLat, m_centerLon);
}

QPointF MapViewPlugin::latLonToWorldPixel(qreal lat, qreal lon) const {
    return map_math::latLonToWorldPixel(lat, lon, m_zoom, TILE_SIZE);
}

QPointF MapViewPlugin::tileToScene(int x, int y) const {
    const QPointF center = latLonToWorldPixel(m_centerLat, m_centerLon);
    return QPointF(x * TILE_SIZE - center.x(), y * TILE_SIZE - center.y());
}

QRectF MapViewPlugin::visibleSceneRect() const {
    if (!m_view) return QRectF();
    return m_view->mapToScene(m_view->viewport()->rect()).boundingRect();
}

void MapViewPlugin::updateVisibleTiles() {
    if (!m_scene || !m_tileLoader) return;

    const QRectF viewport = visibleSceneRect();
    if (viewport.isEmpty()) return;

    const QPointF centerWorld = latLonToWorldPixel(m_centerLat, m_centerLon);
    const QRect range = map_math::visibleTileRange(viewport, centerWorld, m_zoom, TILE_SIZE);

    QSet<QString> newVisible;
    // Request tiles in the visible range
    for (int x = range.left(); x <= range.right(); ++x) {
        for (int y = range.top(); y <= range.bottom(); ++y) {
            QString key = tileKey(x, y);
            newVisible.insert(key);
            requestTile(x, y);
        }
    }
    m_visibleTileKeys = newVisible;

    // Remove tiles that are well outside the visible area
    removeOffscreenTiles(viewport);
    updateTileReadiness();
}

void MapViewPlugin::removeOffscreenTiles(const QRectF& viewport) {
    if (!m_scene) return;

    const qreal margin = TILE_SIZE * 2.0;
    const QRectF marginRect = viewport.adjusted(-margin, -margin, margin, margin);

    auto it = m_tileItems.begin();
    while (it != m_tileItems.end()) {
        QGraphicsPixmapItem* item = it.value();
        const QRectF tileRect(item->pos(), QSizeF(TILE_SIZE, TILE_SIZE));
        if (!marginRect.intersects(tileRect)) {
            m_scene->removeItem(item);
            delete item;
            QString key = it.key();
            it = m_tileItems.erase(it);
            m_loadedTileKeys.remove(key);
            m_visibleTileKeys.remove(key);
        } else {
            ++it;
        }
    }
}

void MapViewPlugin::updateTiles() {
    if (!m_scene || !m_tileLoader) return;

    const QPointF center = latLonToWorldPixel(m_centerLat, m_centerLon);
    const int centerTileX = qFloor(center.x() / TILE_SIZE);
    const int centerTileY = qFloor(center.y() / TILE_SIZE);
    const int maxTile = (1 << m_zoom) - 1;

    QSet<QString> newVisible;
    for (int dx = -TILE_RADIUS; dx <= TILE_RADIUS; ++dx) {
        for (int dy = -TILE_RADIUS; dy <= TILE_RADIUS; ++dy) {
            const int tileX = centerTileX + dx;
            const int tileY = centerTileY + dy;
            if (tileX < 0 || tileX > maxTile || tileY < 0 || tileY > maxTile) continue;
            QString key = tileKey(tileX, tileY);
            newVisible.insert(key);
            requestTile(tileX, tileY);
        }
    }
    m_visibleTileKeys = newVisible;
    updateTileReadiness();
}

void MapViewPlugin::requestTile(int x, int y) {
    const QString key = tileKey(x, y);
    if (m_tileItems.contains(key)) {
        m_tileItems.value(key)->setPos(tileToScene(x, y));
        return;
    }

    QPixmap empty(TILE_SIZE, TILE_SIZE);
    empty.fill(QColor(aegis::ui::ThemeEngine::COLOR_SURFACE));
    auto* placeholder = m_scene->addPixmap(empty);
    placeholder->setPos(tileToScene(x, y));
    placeholder->setZValue(0);
    m_tileItems.insert(key, placeholder);

    aegis::map::TileCoord coord;
    coord.z = m_zoom;
    coord.x = x;
    coord.y = y;
    m_tileLoader->requestTile(coord);
}

void MapViewPlugin::fitViewToVehicle() {
    if (!m_vehicleMarker) return;
    m_view->centerOn(m_vehicleMarker);
}

void MapViewPlugin::zoomIn() {
    if (m_zoom >= 19) return;
    applyZoom(m_zoom + 1);
}

void MapViewPlugin::zoomOut() {
    if (m_zoom <= 1) return;
    applyZoom(m_zoom - 1);
}

void MapViewPlugin::applyZoom(int newZoom) {
    if (newZoom == m_zoom) return;
    m_zoom = newZoom;

    // 1. Cancel pending tile requests and remove tile items
    for (auto* item : m_tileItems) {
        m_scene->removeItem(item);
        delete item;
    }
    m_tileItems.clear();
    m_visibleTileKeys.clear();
    m_loadedTileKeys.clear();

    // 2. Remove heading line
    if (m_headingLine) {
        m_scene->removeItem(m_headingLine);
        delete m_headingLine;
        m_headingLine = nullptr;
    }

    // 3. Update scene extent
    const qreal sceneExtent = TILE_SIZE * (TILE_RADIUS * 2 + 3);
    m_scene->setSceneRect(-sceneExtent / 2, -sceneExtent / 2, sceneExtent, sceneExtent);

    // 4. Update viewport for TileLoader
    aegis::map::ViewportInfo vp;
    vp.zoom = m_zoom;
    if (m_view) {
        vp.widthPx = m_view->viewport()->width();
        vp.heightPx = m_view->viewport()->height();
    }
    m_tileLoader->setViewport(vp);

    // 5. Reposition vehicle
    if (m_hasFix) {
        QPointF pos = latLonToScene(m_lastLat, m_lastLon);
        m_vehicleMarker->setPos(pos);
        updateVehicleMarker(m_lastLat, m_lastLon, m_lastHeading);
    }

    // 6. Recompute track line
    QPainterPath path;
    if (!m_trackHistoryLatLon.isEmpty()) {
        path.moveTo(latLonToScene(m_trackHistoryLatLon.first().first,
                                   m_trackHistoryLatLon.first().second));
        for (int i = 1; i < m_trackHistoryLatLon.size(); ++i) {
            path.lineTo(latLonToScene(m_trackHistoryLatLon[i].first,
                                       m_trackHistoryLatLon[i].second));
        }
    }
    m_trackLine->setPath(path);

    // 7. Load tiles at new zoom
    updateTiles();
}

void MapViewPlugin::onTileReady(const aegis::map::TileCoord& coord, const QPixmap& pixmap) {
    QString key = tileKey(coord.x, coord.y);
    if (m_tileItems.contains(key)) {
        m_tileItems.value(key)->setPixmap(pixmap);
    }
    m_loadedTileKeys.insert(key);
    updateTileReadiness();
}

void MapViewPlugin::onTileFailed(const aegis::map::TileCoord& coord, const QString& error) {
    Q_UNUSED(error)
    QString key = tileKey(coord.x, coord.y);
    // Mark as "loaded" with the placeholder so we don't suspend forever on a permanent failure
    m_loadedTileKeys.insert(key);
    updateTileReadiness();
}

void MapViewPlugin::updateTileReadiness() {
    if (m_visibleTileKeys.isEmpty()) {
        publishTileReadiness(true);
        return;
    }
    bool allLoaded = true;
    for (const QString& key : m_visibleTileKeys) {
        if (!m_loadedTileKeys.contains(key)) {
            allLoaded = false;
            break;
        }
    }
    if (m_followController) {
        m_followController->setViewportFullyLoaded(allLoaded);
    }
    publishTileReadiness(allLoaded);
}

void MapViewPlugin::publishTileReadiness(bool ready) {
    if (!m_bus) return;
    m_bus->publish(QStringLiteral("map/tilesReady"), QVariant(ready));
}

void MapViewPlugin::onFollowStateChanged(FollowState state) {
    if (!m_recenterBtn || !m_statusLabel) return;

    switch (state) {
    case FollowState::Active:
        m_recenterBtn->setVisible(false);
        m_statusLabel->setText(QString());
        break;
    case FollowState::Suspended:
        m_recenterBtn->setVisible(false);
        m_statusLabel->setText(tr("LOADING TILES..."));
        break;
    case FollowState::Disabled:
        m_recenterBtn->setVisible(true);
        m_statusLabel->setText(QString());
        break;
    }
}

void MapViewPlugin::onRecenterClicked() {
    if (m_followController) {
        m_followController->requestRecenter();
    }
}

QString MapViewPlugin::tileKey(int x, int y) const {
    return QStringLiteral("%1/%2/%3").arg(m_zoom).arg(x).arg(y);
}

} // namespace aegis::plugins
