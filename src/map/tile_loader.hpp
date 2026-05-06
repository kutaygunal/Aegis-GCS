#pragma once

#include "tile_types.hpp"
#include "tile_cache.hpp"
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QJsonObject>
#include <QHash>
#include <QSet>

namespace aegis::map {

struct TileFetchTask {
    TileCoord coord;
    int priority = 0; // lower = higher priority (closer to center)
    int retryCount = 0;
};

/**
 * @brief Concurrent tile loader with LRU cache, priority queue, and retry.
 */
class TileLoader : public QObject {
    Q_OBJECT
public:
    explicit TileLoader(QObject* parent = nullptr);
    ~TileLoader();

    void setConfig(const QJsonObject& config);
    void setViewport(const ViewportInfo& vp);

    void requestTile(const TileCoord& coord);
    void cancelTile(const TileCoord& coord);
    void clearCache();

    TileCache* cache() { return &m_cache; }
    TileMetrics metrics() const;

signals:
    void tileReady(const TileCoord& coord, const QPixmap& pixmap);
    void tileFailed(const TileCoord& coord, const QString& error);
    void metricsUpdated(const TileMetrics& metrics);

private slots:
    void onReplyFinished(QNetworkReply* reply);
    void processQueue();

private:
    QString urlForTile(const TileCoord& coord) const;
    int priorityForTile(const TileCoord& coord) const;
    void startDownload(const TileCoord& coord);
    void finishDownload(QNetworkReply* reply, const TileCoord& coord,
                        bool fromCache, qint64 latencyMs);
    void maybeRetry(const TileCoord& coord);
    bool isRetryableError(QNetworkReply::NetworkError error) const;

    QNetworkAccessManager* m_network = nullptr;
    TileCache m_cache;

    QString m_urlTemplate;
    int m_maxConcurrent = 6;
    int m_maxRetries = 2;

    ViewportInfo m_viewport;
    QList<TileFetchTask> m_pending;
    QHash<TileCoord, QNetworkReply*> m_active;
    QSet<TileCoord> m_cancelled; // tiles that were cancelled while in-flight

    // Metrics
    int m_totalHits = 0;
    int m_totalMisses = 0;
    int m_totalFailures = 0;
    qint64 m_totalLatencyMs = 0;
    int m_completedDownloads = 0;
};

} // namespace aegis::map
