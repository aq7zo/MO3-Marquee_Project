#include "Process.h"

#include <algorithm>

namespace mc {

Process::Process(std::string name, int periodMs)
    : name_(std::move(name)), periodMs_(std::max(1, periodMs)) {}

Process::~Process() {
    requestStop();
    join();
}

void Process::start() {
    if (thread_.joinable()) return;
    stop_.store(false);
    thread_ = std::thread(&Process::run, this);
}

void Process::requestStop() {
    {
        std::lock_guard<std::mutex> lock(waitMutex_);
        stop_.store(true);
    }
    waitCv_.notify_all();
}

void Process::join() {
    if (thread_.joinable()) thread_.join();
}

void Process::setPeriodMs(int ms) {
    {
        std::lock_guard<std::mutex> lock(waitMutex_);
        periodMs_.store(std::max(1, ms));
        periodDirty_.store(true);
    }
    waitCv_.notify_all();
}

void Process::run() {
    onStart();

    auto next = std::chrono::steady_clock::now();
    while (!stopRequested()) {
        tick();
        if (stopRequested()) break;

        if (blocksInTick()) continue;

        next += std::chrono::milliseconds(periodMs());

        // If a slow tick pushed us past the deadline, resynchronise instead of
        // trying to catch up - otherwise the loop would spin through a backlog
        // of missed deadlines with no sleep at all.
        const auto now = std::chrono::steady_clock::now();
        if (next < now) next = now;

        std::unique_lock<std::mutex> lock(waitMutex_);
        waitCv_.wait_until(lock, next, [this] {
            return stop_.load(std::memory_order_relaxed) ||
                   periodDirty_.load(std::memory_order_relaxed);
        });
        if (periodDirty_.exchange(false)) {
            next = std::chrono::steady_clock::now();
        }
    }

    onStop();
}

}  // namespace mc
