#pragma once

#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QtMath>

namespace aegis::plugins::map_math {

/**
 * @brief Pure Web-Mercator coordinate math for tile loading.
 *
 * No Qt GUI or network dependencies. Testable without QApplication.
 */

inline QPointF latLonToWorldPixel(qreal lat, qreal lon, int zoom, int tileSize) {
    const qreal clampedLat = qBound(-85.05112878, lat, 85.05112878);
    const qreal sinLat = qSin(qDegreesToRadians(clampedLat));
    const qreal mapSize = static_cast<qreal>(tileSize) * (1 << zoom);

    constexpr qreal PI = 3.14159265358979323846;
    const qreal x = (lon + 180.0) / 360.0 * mapSize;
    const qreal y = (0.5 - qLn((1.0 + sinLat) / (1.0 - sinLat)) / (4.0 * PI)) * mapSize;
    return QPointF(x, y);
}

inline QPoint worldPixelToTile(qreal wx, qreal wy, int tileSize) {
    return QPoint(qFloor(wx / tileSize), qFloor(wy / tileSize));
}

/**
 * @brief Compute the inclusive tile range visible in a scene rectangle.
 *
 * @param sceneRect        Viewport rectangle in scene coordinates.
 * @param centerWorldPixel World pixel coordinate of the scene origin (0,0).
 * @param zoom             Current zoom level.
 * @param tileSize         Tile dimension in pixels (typically 256).
 * @return QRect with left/top = min tile, right/bottom = max tile (inclusive).
 */
inline QRect visibleTileRange(const QRectF& sceneRect,
                              const QPointF& centerWorldPixel,
                              int zoom,
                              int tileSize) {
    // Scene point = worldPixel - centerWorldPixel
    // Therefore worldPixel = scenePoint + centerWorldPixel
    const qreal minWx = sceneRect.left()   + centerWorldPixel.x();
    const qreal maxWx = sceneRect.right()  + centerWorldPixel.x();
    const qreal minWy = sceneRect.top()    + centerWorldPixel.y();
    const qreal maxWy = sceneRect.bottom() + centerWorldPixel.y();

    const QPoint minTile = worldPixelToTile(minWx, minWy, tileSize);
    const QPoint maxTile = worldPixelToTile(maxWx, maxWy, tileSize);

    const int maxTileIdx = (1 << zoom) - 1;

    QRect range;
    range.setLeft(qMax(0, minTile.x()));
    range.setTop(qMax(0, minTile.y()));
    range.setRight(qMin(maxTileIdx, maxTile.x()));
    range.setBottom(qMin(maxTileIdx, maxTile.y()));
    return range;
}

} // namespace aegis::plugins::map_math
