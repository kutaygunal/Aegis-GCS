#pragma once

#include "core/interfaces/itelemetry_sink.hpp"
#include "core/types/common.hpp"
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
#include <QGraphicsTextItem>

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

private slots:
    void onPositionChanged(const core::types::PositionData& data);
    void onMissionCurrentChanged(int index);
    void zoomIn();
    void zoomOut();

private:
    void buildUi();
    void setupScene();
    void updateVehicleMarker(qreal lat, qreal lon, qreal hdg);
    QPointF latLonToScene(qreal lat, qreal lon) const;
    void fitViewToVehicle();

    core::VehicleState* m_state{nullptr};

    QWidget* m_widget{nullptr};
    QGraphicsView* m_view{nullptr};
    QGraphicsScene* m_scene{nullptr};

    // Map items
    QGraphicsEllipseItem* m_vehicleMarker{nullptr};
    QGraphicsPathItem* m_trackLine{nullptr};
    QGraphicsTextItem* m_coordLabel{nullptr};
    QVector<QPointF> m_trackHistory;

    // View state
    qreal m_scale{1.0};
    qreal m_centerLat{37.7749};
    qreal m_centerLon{-122.4194};
    static constexpr qreal SCALE_FACTOR = 100000.0;  // deg-to-scene units
    static constexpr int MAX_TRACK_POINTS = 500;
};

} // namespace aegis::plugins
