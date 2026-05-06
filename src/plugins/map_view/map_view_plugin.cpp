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
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
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
    m_zoom = qBound(1, config.value("zoom", 15).toInt(), 19);
    m_tileUrlTemplate = config.value(
        "tileUrlTemplate",
        QStringLiteral("https://tile.openstreetmap.org/%1/%2/%3.png")).toString();
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
}

void MapViewPlugin::setupScene() {
    if (!m_network) {
        m_network = new QNetworkAccessManager(this);
    }

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
    fitViewToVehicle();
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
    if (!m_scene || !m_network) return;

    const QRectF viewport = visibleSceneRect();
    if (viewport.isEmpty()) return;

    const QPointF centerWorld = latLonToWorldPixel(m_centerLat, m_centerLon);
    const QRect range = map_math::visibleTileRange(viewport, centerWorld, m_zoom, TILE_SIZE);

    // Request tiles in the visible range
    for (int x = range.left(); x <= range.right(); ++x) {
        for (int y = range.top(); y <= range.bottom(); ++y) {
            requestTile(x, y);
        }
    }

    // Remove tiles that are well outside the visible area
    removeOffscreenTiles(viewport);
}

void MapViewPlugin::removeOffscreenTiles(const QRectF& viewport) {
    if (!m_scene) return;

    // Margin of 2 tiles to avoid thrashing during small pans
    const qreal margin = TILE_SIZE * 2.0;
    const QRectF marginRect = viewport.adjusted(-margin, -margin, margin, margin);

    auto it = m_tileItems.begin();
    while (it != m_tileItems.end()) {
        QGraphicsPixmapItem* item = it.value();
        const QRectF tileRect(item->pos(), QSizeF(TILE_SIZE, TILE_SIZE));
        if (!marginRect.intersects(tileRect)) {
            m_scene->removeItem(item);
            delete item;
            it = m_tileItems.erase(it);
        } else {
            ++it;
        }
    }
}

void MapViewPlugin::updateTiles() {
    if (!m_scene || !m_network) return;

    const QPointF center = latLonToWorldPixel(m_centerLat, m_centerLon);
    const int centerTileX = qFloor(center.x() / TILE_SIZE);
    const int centerTileY = qFloor(center.y() / TILE_SIZE);
    const int maxTile = (1 << m_zoom) - 1;

    for (int dx = -TILE_RADIUS; dx <= TILE_RADIUS; ++dx) {
        for (int dy = -TILE_RADIUS; dy <= TILE_RADIUS; ++dy) {
            const int tileX = centerTileX + dx;
            const int tileY = centerTileY + dy;
            if (tileX < 0 || tileX > maxTile || tileY < 0 || tileY > maxTile) continue;
            requestTile(tileX, tileY);
        }
    }
}

void MapViewPlugin::requestTile(int x, int y) {
    const QString key = QStringLiteral("%1/%2/%3").arg(m_zoom).arg(x).arg(y);
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

    const QUrl url(m_tileUrlTemplate.arg(m_zoom).arg(x).arg(y));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("AEGIS-GCS/0.1 (+https://github.com/aegis-gcs)"));
    auto* reply = m_network->get(request);
    reply->setProperty("tileKey", key);
    connect(reply, &QNetworkReply::finished, this, &MapViewPlugin::onTileDownloaded);
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

    // 1. Remove all tile items (but keep vehicle, track, crosshair)
    for (auto* item : m_tileItems) {
        m_scene->removeItem(item);
        delete item;
    }
    m_tileItems.clear();

    // 2. Remove heading line (will be recreated by updateVehicleMarker)
    if (m_headingLine) {
        m_scene->removeItem(m_headingLine);
        delete m_headingLine;
        m_headingLine = nullptr;
    }

    // 3. Update scene extent for new zoom
    const qreal sceneExtent = TILE_SIZE * (TILE_RADIUS * 2 + 3);
    m_scene->setSceneRect(-sceneExtent / 2, -sceneExtent / 2, sceneExtent, sceneExtent);

    // 4. Reposition vehicle marker + heading with new zoom math
    if (m_hasFix) {
        QPointF pos = latLonToScene(m_lastLat, m_lastLon);
        m_vehicleMarker->setPos(pos);
        updateVehicleMarker(m_lastLat, m_lastLon, m_lastHeading);
    }

    // 5. Recompute track line with new zoom (reproject all historical points)
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

    // 6. Load tiles at new zoom
    updateTiles();
    if (m_hasFix) fitViewToVehicle();
}

void MapViewPlugin::onTileDownloaded() {
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    const QString key = reply->property("tileKey").toString();
    if (reply->error() == QNetworkReply::NoError && m_tileItems.contains(key)) {
        QPixmap pixmap;
        pixmap.loadFromData(reply->readAll());
        if (!pixmap.isNull()) {
            m_tileItems.value(key)->setPixmap(pixmap);
        }
    } else if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "[MapView] Tile download failed:" << reply->url() << reply->errorString();
    }

    reply->deleteLater();
}

} // namespace aegis::plugins
