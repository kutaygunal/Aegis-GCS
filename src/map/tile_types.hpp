#pragma once

#include <QHash>
#include <QPixmap>
#include <QString>

namespace aegis::map {

struct TileCoord {
    int z = 0;
    int x = 0;
    int y = 0;

    bool operator==(const TileCoord& other) const noexcept {
        return z == other.z && x == other.x && y == other.y;
    }
};

inline uint qHash(const TileCoord& key, uint seed = 0) noexcept {
    return qHashMulti(seed, key.z, key.x, key.y);
}

struct ViewportInfo {
    double centerLat = 0.0;
    double centerLon = 0.0;
    int zoom = 0;
    int widthPx = 0;
    int heightPx = 0;
};

struct TileMetrics {
    int hits = 0;
    int misses = 0;
    int failures = 0;
    double avgLatencyMs = 0.0;
    int pendingCount = 0;
    int activeDownloads = 0;
};

} // namespace aegis::map
