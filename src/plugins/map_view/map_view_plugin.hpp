#pragma once

#include "core/interfaces/itelemetry_sink.hpp"
#include "core/types/common.hpp"
#include "map_math.hpp"
#include "follow_vehicle_controller.hpp"
#include "map/tile_loader.hpp"
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
#include <QGraphicsLineItem>
#include <QGraphicsPixmapItem>
#include <QLabel>
#include <QPushButton>
#include <QHash>
#include <QSet>

namespace aegis::plugins {

/**
 * @brief 2D geospatial map view with vehicle track and waypoint overlay.
 *
 * Native Qt implementation (QGraphicsView) when Qt WebEngine is unavailable.
 * Upgrades to CesiumJS globe automatically when built with WebEngine.
 */
class MapViewPlugin : public core::ITelemetrySink {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.aegis.gcs.IPlugin/1.0" FILE "map_view.json")
    Q_INTERFACES(aegis::core::IPlugin)

public:
    explicit MapViewPlugin(QObject* parent = nullptr);
    ~MapViewPlugin() override;

    bool initialize(core::TelemetryBus* bus, core::VehicleState* state,
                    const QVariantMap& config) override;
    QWidget* widget() override;
    void shutdown() override;

    QString name() const override      { return tr("Map View"); }
    QString pluginId() const override  { return QStringLiteral("aegis.plugins.map_view"); }
    QString version() const override   { return QStringLiteral("1.0.0"); }
    QString author() const override  { return QStringLiteral("AEGIS Team"); }
    QString category() const override{ return QStringLiteral("Flight"); }

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onPositionChanged(const core::types::PositionData& data);
    void onMissionCurrentChanged(int index);
    void zoomIn();
    void zoomOut();
    void applyZoom(int newZoom);
    void onTileReady(const aegis::map::TileCoord& coord, const QPixmap& pixmap);
    void onTileFailed(const aegis::map::TileCoord& coord, const QString& error);
    void onFollowStateChanged(FollowState state);
    void onRecenterClicked();

private:
    void buildUi();
    void setupScene();
    void updateVehicleMarker(qreal lat, qreal lon, qreal hdg);
    QPointF latLonToScene(qreal lat, qreal lon) const;
    QPointF latLonToWorldPixel(qreal lat, qreal lon) const;
    QPointF tileToScene(int x, int y) const;
    void updateTiles();
    void updateVisibleTiles();
    void removeOffscreenTiles(const QRectF& viewport);
    QRectF visibleSceneRect() const;
    void requestTile(int x, int y);
    void fitViewToVehicle();
    void updateTileReadiness();
    void publishTileReadiness(bool ready);
    QString tileKey(int x, int y) const;

    core::VehicleState* m_state{nullptr};
    core::TelemetryBus* m_bus{nullptr};

    QWidget* m_widget{nullptr};
    QGraphicsView* m_view{nullptr};
    QGraphicsScene* m_scene{nullptr};

    // Map items
    QGraphicsEllipseItem* m_vehicleMarker{nullptr};
    QGraphicsPathItem* m_trackLine{nullptr};
    QLabel* m_coordLabel{nullptr};
    QGraphicsLineItem* m_headingLine{nullptr};
    QVector<QPair<qreal, qreal>> m_trackHistoryLatLon;
    QHash<QString, QGraphicsPixmapItem*> m_tileItems;

    // Tile loading
    aegis::map::TileLoader* m_tileLoader{nullptr};
    QSet<QString> m_visibleTileKeys;
    QSet<QString> m_loadedTileKeys;

    // Follow-vehicle
    FollowVehicleController* m_followController{nullptr};
    QPushButton* m_recenterBtn{nullptr};
    QLabel* m_statusLabel{nullptr};

    // View state
    int m_zoom{15};
    qreal m_centerLat{37.7749};
    qreal m_centerLon{-122.4194};
    bool m_hasFix{false};
    qreal m_lastLat{0.0};
    qreal m_lastLon{0.0};
    qreal m_lastHeading{0.0};
    QString m_tileUrlTemplate{QStringLiteral("https://tile.openstreetmap.org/%1/%2/%3.png")};
    static constexpr int TILE_SIZE = 256;
    static constexpr int TILE_RADIUS = 2;
    static constexpr int MAX_TRACK_POINTS = 500;
};

} // namespace aegis::plugins
