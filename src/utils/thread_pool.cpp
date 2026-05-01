#include "thread_pool.hpp"

namespace aegis::utils {

ThreadPool& ThreadPool::instance() {
    static ThreadPool inst;
    return inst;
}

ThreadPool::ThreadPool() {
    m_pool.setMaxThreadCount(QThread::idealThreadCount());
}

void ThreadPool::setMaxThreads(int n) {
    m_pool.setMaxThreadCount(n);
}

int ThreadPool::maxThreads() const {
    return m_pool.maxThreadCount();
}

void ThreadPool::clearQueue() {
    m_pool.clear();
}

} // namespace aegis::utils
