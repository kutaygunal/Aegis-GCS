#pragma once

#include <QMutex>
#include <QVector>
#include <atomic>

namespace aegis::utils {

/**
 * @brief Thread-safe fixed-size ring buffer for telemetry history.
 *
 * Used by the Mission Editor and Alert Console for back-in-time analysis
 * without unbounded heap growth during long-duration missions.
 */
template <typename T>
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity) : m_buffer(capacity), m_capacity(capacity) {}

    void push(const T& item) {
        QMutexLocker lock(&m_mutex);
        m_buffer[m_writeIdx % m_capacity] = item;
        ++m_writeIdx;
        if (m_writeIdx > m_capacity) m_readIdx = m_writeIdx - m_capacity;
    }

    QVector<T> snapshot() const {
        QMutexLocker lock(&m_mutex);
        QVector<T> result;
        result.reserve(qMin(m_writeIdx, m_capacity));
        const size_t start = m_writeIdx > m_capacity ? m_writeIdx - m_capacity : 0;
        for (size_t i = start; i < m_writeIdx; ++i) {
            result.append(m_buffer[i % m_capacity]);
        }
        return result;
    }

    size_t size() const {
        QMutexLocker lock(&m_mutex);
        return qMin(m_writeIdx, m_capacity);
    }

    void clear() {
        QMutexLocker lock(&m_mutex);
        m_writeIdx = 0;
        m_readIdx = 0;
    }

private:
    mutable QMutex m_mutex;
    QVector<T> m_buffer;
    size_t m_capacity{0};
    size_t m_writeIdx{0};
    size_t m_readIdx{0};
};

} // namespace aegis::utils
