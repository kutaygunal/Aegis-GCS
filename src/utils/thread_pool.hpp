#pragma once

#include <QThreadPool>
#include <QRunnable>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QMutex>

namespace aegis::utils {

/**
 * @brief Thin wrapper around QThreadPool for telemetry-heavy background tasks.
 *
 * Used for log parsing, map tile fetching, and sensor data preprocessing.
 */
class ThreadPool {
public:
    static ThreadPool& instance();

    void setMaxThreads(int n);
    int maxThreads() const;

    template <typename Func>
    auto run(Func&& f) -> QFuture<std::invoke_result_t<Func>> {
        return QtConcurrent::run(&m_pool, std::forward<Func>(f));
    }

    void clearQueue();

private:
    ThreadPool();
    QThreadPool m_pool;
};

} // namespace aegis::utils
