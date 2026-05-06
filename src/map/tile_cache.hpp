#pragma once

#include "tile_types.hpp"
#include <QMutex>
#include <QMutexLocker>
#include <QImage>
#include <QHash>
#include <list>

namespace aegis::map {

/**
 * @brief Thread-safe LRU tile cache with byte-size eviction.
 */
class TileCache {
public:
    explicit TileCache(size_t maxBytes = 256 * 1024 * 1024)
        : m_maxBytes(maxBytes) {}

    void setMaxBytes(size_t maxBytes) {
        QMutexLocker lock(&m_mutex);
        m_maxBytes = maxBytes;
        evictIfNeeded();
    }

    size_t maxBytes() const {
        QMutexLocker lock(&m_mutex);
        return m_maxBytes;
    }

    size_t currentBytes() const {
        QMutexLocker lock(&m_mutex);
        return m_currentBytes;
    }

    bool get(const TileCoord& coord, QImage& out) {
        QMutexLocker lock(&m_mutex);
        auto it = m_lookup.find(coord);
        if (it == m_lookup.end()) {
            ++m_misses;
            return false;
        }
        // Move to front (most recently used)
        m_order.splice(m_order.begin(), m_order, it.value());
        out = it.value()->second;
        ++m_hits;
        return true;
    }

    void put(const TileCoord& coord, const QImage& image) {
        QMutexLocker lock(&m_mutex);
        auto it = m_lookup.find(coord);
        if (it != m_lookup.end()) {
            // Update existing
            m_currentBytes -= imageBytes(it.value()->second);
            it.value()->second = image;
            m_currentBytes += imageBytes(image);
            m_order.splice(m_order.begin(), m_order, it.value());
        } else {
            m_order.emplace_front(coord, image);
            m_lookup.insert(coord, m_order.begin());
            m_currentBytes += imageBytes(image);
        }
        evictIfNeeded();
    }

    void clear() {
        QMutexLocker lock(&m_mutex);
        m_order.clear();
        m_lookup.clear();
        m_currentBytes = 0;
        m_hits = 0;
        m_misses = 0;
    }

    double hitRate() const {
        QMutexLocker lock(&m_mutex);
        const int total = m_hits + m_misses;
        return total > 0 ? static_cast<double>(m_hits) / total : 0.0;
    }

    int hitCount() const {
        QMutexLocker lock(&m_mutex);
        return m_hits;
    }

    int missCount() const {
        QMutexLocker lock(&m_mutex);
        return m_misses;
    }

private:
    using Entry = std::pair<TileCoord, QImage>;

    mutable QMutex m_mutex;
    size_t m_maxBytes = 0;
    size_t m_currentBytes = 0;
    int m_hits = 0;
    int m_misses = 0;
    std::list<Entry> m_order;
    QHash<TileCoord, typename std::list<Entry>::iterator> m_lookup;

    static size_t imageBytes(const QImage& img) {
        return static_cast<size_t>(img.sizeInBytes());
    }

    void evictIfNeeded() {
        while (m_currentBytes > m_maxBytes && !m_order.empty()) {
            const auto& oldest = m_order.back();
            m_currentBytes -= imageBytes(oldest.second);
            m_lookup.remove(oldest.first);
            m_order.pop_back();
        }
    }
};

} // namespace aegis::map
