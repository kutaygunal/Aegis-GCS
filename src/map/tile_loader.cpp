#include "tile_loader.hpp"
#include <QElapsedTimer>
#include <QDebug>
#include <QtMath>

namespace aegis::map {

TileLoader::TileLoader(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    connect(m_network, &QNetworkAccessManager::finished,
            this, &TileLoader::onReplyFinished);
}

TileLoader::~TileLoader() = default;

void TileLoader::setConfig(const QJsonObject& config) {
    m_urlTemplate = config.value(QStringLiteral("tileUrlTemplate")).toString();
    if (m_urlTemplate.isEmpty()) {
        m_urlTemplate = QStringLiteral("https://tile.openstreetmap.org/%1/%2/%3.png");
    }
    m_maxConcurrent = qMax(1, config.value(QStringLiteral("maxConcurrentDownloads")).toInt(6));
    m_maxRetries = qMax(0, config.value(QStringLiteral("maxRetries")).toInt(2));
    const int cacheSizeMB = qMax(1, config.value(QStringLiteral("tileCacheSizeMB")).toInt(256));
    m_cache.setMaxBytes(static_cast<size_t>(cacheSizeMB) * 1024 * 1024);
}

void TileLoader::setViewport(const ViewportInfo& vp) {
    m_viewport = vp;
    // Re-sort pending queue by new priority
    for (auto& task : m_pending) {
        task.priority = priorityForTile(task.coord);
    }
    std::sort(m_pending.begin(), m_pending.end(),
              [](const TileFetchTask& a, const TileFetchTask& b) {
                  return a.priority < b.priority;
              });
    // Cancel tiles that are now far outside viewport
    QSet<TileCoord> visibleSet;
    // Rough visibility: within 2 tiles of viewport center
    // (exact bounds computed by caller in updateVisibleTiles)
    Q_UNUSED(visibleSet)
    emit metricsUpdated(metrics());
}

void TileLoader::requestTile(const TileCoord& coord) {
    // Check cache first
    QImage cached;
    if (m_cache.get(coord, cached)) {
        ++m_totalHits;
        emit tileReady(coord, QPixmap::fromImage(cached));
        emit metricsUpdated(metrics());
        return;
    }

    // Already pending or active?
    for (const auto& t : m_pending) {
        if (t.coord == coord) return;
    }
    if (m_active.contains(coord)) return;

    ++m_totalMisses;
    TileFetchTask task;
    task.coord = coord;
    task.priority = priorityForTile(coord);
    m_pending.append(task);
    std::sort(m_pending.begin(), m_pending.end(),
              [](const TileFetchTask& a, const TileFetchTask& b) {
                  return a.priority < b.priority;
              });
    processQueue();
}

void TileLoader::cancelTile(const TileCoord& coord) {
    // Remove from pending
    for (int i = 0; i < m_pending.size(); ++i) {
        if (m_pending[i].coord == coord) {
            m_pending.removeAt(i);
            break;
        }
    }
    // Mark active as cancelled; result will be discarded
    if (m_active.contains(coord)) {
        m_cancelled.insert(coord);
    }
    emit metricsUpdated(metrics());
}

void TileLoader::clearCache() {
    m_cache.clear();
}

TileMetrics TileLoader::metrics() const {
    TileMetrics m;
    m.hits = m_totalHits;
    m.misses = m_totalMisses;
    m.failures = m_totalFailures;
    m.pendingCount = m_pending.size();
    m.activeDownloads = m_active.size();
    if (m_completedDownloads > 0) {
        m.avgLatencyMs = static_cast<double>(m_totalLatencyMs) / m_completedDownloads;
    }
    return m;
}

QString TileLoader::urlForTile(const TileCoord& coord) const {
    return m_urlTemplate.arg(coord.z).arg(coord.x).arg(coord.y);
}

int TileLoader::priorityForTile(const TileCoord& coord) const {
    // Distance from viewport center in tile coordinates
    // Approximate center tile from lat/lon
    // (exact math not needed; plugin provides tile coords directly)
    const int cx = m_viewport.widthPx / 2;
    const int cy = m_viewport.heightPx / 2;
    // Tile-to-pixel scale depends on zoom; use rough heuristic
    const int tileSize = 256;
    const int centerTileX = cx / tileSize;
    const int centerTileY = cy / tileSize;
    return qAbs(coord.x - centerTileX) + qAbs(coord.y - centerTileY);
}

void TileLoader::processQueue() {
    while (m_active.size() < m_maxConcurrent && !m_pending.isEmpty()) {
        TileFetchTask task = m_pending.takeFirst();
        startDownload(task.coord);
    }
    emit metricsUpdated(metrics());
}

void TileLoader::startDownload(const TileCoord& coord) {
    const QUrl url(urlForTile(coord));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("AEGIS-GCS/0.1 (+https://github.com/aegis-gcs)"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = m_network->get(request);
    reply->setProperty("aegisTileCoord",
                         QVariant::fromValue(coord));
    reply->setProperty("aegisStartTime",
                         QVariant::fromValue(QDateTime::currentMSecsSinceEpoch()));
    m_active.insert(coord, reply);
}

void TileLoader::onReplyFinished(QNetworkReply* reply) {
    if (!reply) return;

    const TileCoord coord = reply->property("aegisTileCoord").value<TileCoord>();
    const qint64 startMs = reply->property("aegisStartTime").toLongLong();
    const qint64 latencyMs = QDateTime::currentMSecsSinceEpoch() - startMs;

    m_active.remove(coord);
    ++m_completedDownloads;
    m_totalLatencyMs += latencyMs;

    // Was this tile cancelled while in flight?
    if (m_cancelled.remove(coord)) {
        reply->deleteLater();
        processQueue();
        return;
    }

    if (reply->error() == QNetworkReply::NoError) {
        QImage image;
        image.loadFromData(reply->readAll());
        if (!image.isNull()) {
            m_cache.put(coord, image);
            emit tileReady(coord, QPixmap::fromImage(image));
        } else {
            ++m_totalFailures;
            maybeRetry(coord);
        }
    } else {
        ++m_totalFailures;
        if (isRetryableError(reply->error())) {
            maybeRetry(coord);
        } else {
            emit tileFailed(coord, reply->errorString());
        }
    }

    reply->deleteLater();
    processQueue();
}

void TileLoader::maybeRetry(const TileCoord& coord) {
    // Find task to retry or create new
    for (auto& t : m_pending) {
        if (t.coord == coord) {
            if (t.retryCount < m_maxRetries) {
                ++t.retryCount;
                const int backoffMs = qMin(1000 * (1 << t.retryCount), 30000);
                QTimer::singleShot(backoffMs, this, [this, coord]() {
                    // Re-insert into queue if not already there
                    bool alreadyPending = false;
                    for (const auto& t : m_pending) {
                        if (t.coord == coord) { alreadyPending = true; break; }
                    }
                    if (!alreadyPending && !m_active.contains(coord)) {
                        TileFetchTask task;
                        task.coord = coord;
                        task.priority = priorityForTile(coord);
                        task.retryCount = 0; // reset for fresh attempt
                        m_pending.append(task);
                        std::sort(m_pending.begin(), m_pending.end(),
                                  [](const TileFetchTask& a, const TileFetchTask& b) {
                                      return a.priority < b.priority;
                                  });
                        processQueue();
                    }
                });
            } else {
                emit tileFailed(coord, QStringLiteral("Max retries exceeded"));
            }
            return;
        }
    }
    // Not in pending — create retry task
    if (m_maxRetries > 0) {
        TileFetchTask task;
        task.coord = coord;
        task.priority = priorityForTile(coord);
        task.retryCount = 1;
        const int backoffMs = 2000;
        QTimer::singleShot(backoffMs, this, [this, coord]() {
            bool alreadyPending = false;
            for (const auto& t : m_pending) {
                if (t.coord == coord) { alreadyPending = true; break; }
            }
            if (!alreadyPending && !m_active.contains(coord)) {
                TileFetchTask task;
                task.coord = coord;
                task.priority = priorityForTile(coord);
                task.retryCount = 0;
                m_pending.append(task);
                std::sort(m_pending.begin(), m_pending.end(),
                          [](const TileFetchTask& a, const TileFetchTask& b) {
                              return a.priority < b.priority;
                          });
                processQueue();
            }
        });
    } else {
        emit tileFailed(coord, QStringLiteral("Download failed"));
    }
}

bool TileLoader::isRetryableError(QNetworkReply::NetworkError error) const {
    switch (error) {
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::RemoteHostClosedError:
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::TimeoutError:
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::InternalServerError:
    case QNetworkReply::ServiceUnavailableError:
    case QNetworkReply::UnknownServerError:
        return true;
    default:
        return false;
    }
}

} // namespace aegis::map
