#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

namespace mc {

// Base class for a concurrent worker with its own independent period.
//
// This is the "process representation" the technical report asks about: each
// unit of concurrent work in the emulator - animating, rendering, polling the
// keyboard, interpreting commands - is a Process with its own thread, its own
// rate, and a uniform start/stop/join lifecycle.
//
// The wait is a condition_variable timed wait against an absolute deadline, so
// there is no busy-waiting (CPU stays near idle while the marquee animates) and
// no cumulative drift. Because the wait predicate also watches the stop flag, a
// process with a 60-second period still shuts down instantly.
class Process {
public:
    Process(std::string name, int periodMs);
    virtual ~Process();

    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;

    void start();
    void requestStop();
    void join();

    const std::string& name() const { return name_; }
    int periodMs() const { return periodMs_.load(std::memory_order_relaxed); }

    // Takes effect immediately: the current wait is cut short and the cadence
    // restarts, so dropping from a 60s period back to 50ms does not leave the
    // user staring at a frozen marquee for a minute.
    void setPeriodMs(int ms);

protected:
    virtual void onStart() {}
    virtual void tick() = 0;
    virtual void onStop() {}

    // Processes that block inside tick() (waiting on a queue, say) opt out of
    // the periodic sleep.
    virtual bool blocksInTick() const { return false; }

    bool stopRequested() const { return stop_.load(std::memory_order_relaxed); }

private:
    void run();

    std::string name_;
    std::atomic<int> periodMs_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> periodDirty_{false};
    std::thread thread_;
    std::mutex waitMutex_;
    std::condition_variable waitCv_;
};

}  // namespace mc
