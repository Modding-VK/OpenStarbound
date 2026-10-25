#include "StarWorkerPool.hpp"
#include "StarIterator.hpp"
#include "StarMathCommon.hpp"
#include "StarTime.hpp"
#include <mutex>
#include <map>
#include <vector>

namespace {
std::atomic<bool> monitoringEnabled{false};
std::atomic<uint64_t> monitoringGeneration{0};
std::mutex registryMutex;
std::vector<std::weak_ptr<Star::WorkerPool::Monitor>> registry;
}

namespace Star {

bool WorkerPoolHandle::done() const {
  MutexLocker locker(m_impl->mutex);
  return m_impl->done;
}

bool WorkerPoolHandle::wait(unsigned millis) const {
  MutexLocker locker(m_impl->mutex);

  if (!m_impl->done && millis != 0)
    m_impl->condition.wait(m_impl->mutex, millis);

  if (m_impl->exception)
    std::rethrow_exception(m_impl->exception);

  return m_impl->done;
}

bool WorkerPoolHandle::poll() const {
  return wait(0);
}

void WorkerPoolHandle::finish() const {
  MutexLocker locker(m_impl->mutex);

  if (!m_impl->done)
    m_impl->condition.wait(m_impl->mutex);

  if (m_impl->exception)
    std::rethrow_exception(m_impl->exception);

  return;
}

WorkerPoolHandle::Impl::Impl() : done(false) {}

WorkerPoolHandle::WorkerPoolHandle(shared_ptr<Impl> impl) : m_impl(std::move(impl)) {}

WorkerPool::WorkerPool(String name) : m_name(std::move(name)), m_monitor(make_shared<Monitor>(m_name)) {
  std::lock_guard<std::mutex> lock(registryMutex);
  registry.push_back(m_monitor);
}

void WorkerPool::setMonitoringEnabled(bool enabled) {
  if (monitoringEnabled.exchange(enabled, std::memory_order_relaxed) == enabled)
    return;
  monitoringGeneration.fetch_add(1, std::memory_order_relaxed);
  std::lock_guard<std::mutex> lock(registryMutex);
  for (auto const& weak : registry) {
    if (auto monitor = weak.lock()) {
      monitor->submitted = 0;
      monitor->completed = 0;
      monitor->busyMicroseconds = 0;
      monitor->lastObservedMicroseconds = 0;
      monitor->queued = 0;
      monitor->peakQueued = 0;
    }
  }
}

void WorkerPool::recordLightingWork(uint64_t microseconds) {
  recordDedicatedWork("Lighting thread", microseconds);
}

bool WorkerPool::monitoring() {
  return monitoringEnabled.load(std::memory_order_relaxed);
}

void WorkerPool::recordDedicatedWork(String const& name, uint64_t microseconds) {
  if (!monitoringEnabled.load(std::memory_order_relaxed))
    return;
  static std::map<String, shared_ptr<Monitor>> dedicated;
  shared_ptr<Monitor> monitor;
  {
    std::lock_guard<std::mutex> lock(registryMutex);
    auto it = dedicated.find(name);
    if (it == dedicated.end() && dedicated.size() >= 64) {
      auto now = Time::monotonicMicroseconds();
      for (auto old = dedicated.begin(); old != dedicated.end();) {
        if (now - old->second->lastObservedMicroseconds.load() > 300000000)
          old = dedicated.erase(old);
        else
          ++old;
      }
      if (dedicated.size() >= 64)
        return;
    }
    auto& entry = dedicated[name];
    if (!entry) {
      entry = make_shared<Monitor>(name);
      entry->workers = 1;
      entry->dedicated = true;
      registry.push_back(entry);
    }
    monitor = entry;
  }
  monitor->submitted.fetch_add(1);
  monitor->completed.fetch_add(1);
  monitor->busyMicroseconds.fetch_add(microseconds);
  monitor->lastObservedMicroseconds = Time::monotonicMicroseconds();
}

std::vector<WorkerPool::Snapshot> WorkerPool::snapshots() {
  std::vector<Snapshot> result;
  if (!monitoringEnabled.load(std::memory_order_relaxed))
    return result;
  std::lock_guard<std::mutex> lock(registryMutex);
  auto now = Time::monotonicMicroseconds();
  for (auto it = registry.begin(); it != registry.end();) {
    if (auto monitor = it->lock()) {
      if (monitor->dedicated && (monitor->lastObservedMicroseconds == 0
          || now - monitor->lastObservedMicroseconds.load() > 30000000)) {
        ++it;
        continue;
      }
      result.push_back({monitor->name, monitor->workers.load(), monitor->active.load(),
          monitor->submitted.load(), monitor->completed.load(), monitor->busyMicroseconds.load(),
          monitor->queued.load(), monitor->peakQueued.load(), monitor->dedicated});
      ++it;
    } else {
      it = registry.erase(it);
    }
  }
  return result;
}

void WorkerPool::runWork(function<void()>& work) {
  if (!monitoringEnabled.load(std::memory_order_relaxed)) {
    work();
    return;
  }
  m_monitor->active.fetch_add(1);
  auto generation = monitoringGeneration.load(std::memory_order_relaxed);
  auto start = Time::monotonicMicroseconds();
  try {
    work();
  } catch (...) {
    if (monitoringEnabled.load(std::memory_order_relaxed)
        && generation == monitoringGeneration.load(std::memory_order_relaxed)) {
      m_monitor->busyMicroseconds.fetch_add(Time::monotonicMicroseconds() - start);
      m_monitor->completed.fetch_add(1);
    }
    m_monitor->active.fetch_sub(1);
    throw;
  }
  if (monitoringEnabled.load(std::memory_order_relaxed)
      && generation == monitoringGeneration.load(std::memory_order_relaxed)) {
    m_monitor->busyMicroseconds.fetch_add(Time::monotonicMicroseconds() - start);
    m_monitor->completed.fetch_add(1);
  }
  m_monitor->active.fetch_sub(1);
}

WorkerPool::WorkerPool(String name, unsigned threadCount) : WorkerPool(std::move(name)) {
  start(threadCount);
}

WorkerPool::~WorkerPool() {
  stop();
}

WorkerPool::WorkerPool(WorkerPool&&) = default;
WorkerPool& WorkerPool::operator=(WorkerPool&&) = default;

void WorkerPool::start(unsigned threadCount) {
  MutexLocker threadLock(m_threadMutex);

  for (auto const& workerThread : m_workerThreads)
    workerThread->shouldStop = true;

  m_workCondition.broadcast();
  m_workerThreads.clear();

  for (size_t i = m_workerThreads.size(); i < threadCount; ++i)
    m_workerThreads.append(make_unique<WorkerThread>(this));
  m_monitor->workers = threadCount;
}

void WorkerPool::stop() {
  MutexLocker threadLock(m_threadMutex);
  for (auto const& workerThread : m_workerThreads)
    workerThread->shouldStop = true;

  {
    // Must hold the work lock while broadcasting to ensure that any worker
    // threads that might wait without stopping actually get the signal.
    MutexLocker workLock(m_workMutex);
    m_workCondition.broadcast();
  }

  m_workerThreads.clear();
  m_monitor->workers = 0;
}

void WorkerPool::finish() {
  // This is kind of a weird way to "wait" until all the pending work is
  // finished.  In order for the currently active worker threads to
  // cooperatively complete the remaining work, the work lock must not be held
  // the entire time (then just this thread would be the one finishing the
  // work).  Instead, the calling thread joins in on the action and tries to
  // finish work while yielding to the other threads after each completed job.
  MutexLocker workMutex(m_workMutex);
  while (!m_pendingWork.empty()) {
    auto firstWork = m_pendingWork.takeFirst();
    if (monitoringEnabled.load(std::memory_order_relaxed))
      m_monitor->queued = m_pendingWork.size();
    workMutex.unlock();
    runWork(firstWork);
    Thread::yield();
    workMutex.lock();
  }
  workMutex.unlock();

  stop();
}

WorkerPoolHandle WorkerPool::addWork(function<void()> work) {
  // Construct a worker pool handle and wrap the work to signal the handle when
  // finished.  Set the result to empty string if successful and to the content
  // of the exception if an exception is thrown.
  auto workerPoolHandleImpl = make_shared<WorkerPoolHandle::Impl>();
  queueWork([workerPoolHandleImpl, work]() {
    try {
      work();
      MutexLocker handleLocker(workerPoolHandleImpl->mutex);
      workerPoolHandleImpl->done = true;
      workerPoolHandleImpl->condition.broadcast();
    } catch (...) {
      MutexLocker handleLocker(workerPoolHandleImpl->mutex);
      workerPoolHandleImpl->done = true;
      workerPoolHandleImpl->exception = std::current_exception();
      workerPoolHandleImpl->condition.broadcast();
    }
  });

  return workerPoolHandleImpl;
}

size_t WorkerPool::getWorkerCount() const {
  return m_workerThreads.size();
}

WorkerPool::WorkerThread::WorkerThread(WorkerPool* parent)
  : Thread(strf("WorkerThread for WorkerPool '{}'", parent->m_name)),
    parent(parent),
    shouldStop(false),
    waiting(false) {
  start();
}

WorkerPool::WorkerThread::~WorkerThread() {
  join();
}

void WorkerPool::WorkerThread::run() {
  MutexLocker workLock(parent->m_workMutex);
  while (true) {
    if (shouldStop)
      break;

    if (parent->m_pendingWork.empty()) {
      waiting = true;
      parent->m_workCondition.wait(parent->m_workMutex);
      waiting = false;
    }

    if (!parent->m_pendingWork.empty()) {
      auto work = parent->m_pendingWork.takeFirst();
      if (monitoringEnabled.load(std::memory_order_relaxed))
        parent->m_monitor->queued = parent->m_pendingWork.size();
      workLock.unlock();
      parent->runWork(work);
      workLock.lock();
    }
  }
}

void WorkerPool::queueWork(function<void()> work) {
  MutexLocker workLock(m_workMutex);
  m_pendingWork.append(std::move(work));
  if (monitoringEnabled.load(std::memory_order_relaxed)) {
    m_monitor->submitted.fetch_add(1);
    size_t depth = m_pendingWork.size();
    m_monitor->queued = depth;
    size_t peak = m_monitor->peakQueued.load();
    while (depth > peak && !m_monitor->peakQueued.compare_exchange_weak(peak, depth)) {}
  }
  m_workCondition.signal();
}

}
