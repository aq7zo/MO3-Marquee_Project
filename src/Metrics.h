#pragma once

#include <atomic>

namespace mc {

// Lightweight instrumentation behind the `status` command.
//
// These numbers are a graded deliverable, not a debugging aid: the technical
// report has to identify the recommended refresh and polling values for this
// hardware and the point at which tearing or typing lag appear. Measuring them
// live is far more defensible than estimating afterwards.
//
// Values are exponentially smoothed. Races between writers are benign here -
// a slightly stale average is fine and is much cheaper than locking per frame.
class Metrics {
public:
    void recordFrame(double frameMs, double composeMs);
    void recordPoll(double intervalMs);
    void recordInputLatency(double ms);
    void resetPeaks();

    double avgFrameMs() const { return avgFrameMs_.load(std::memory_order_relaxed); }
    double avgComposeMs() const { return avgComposeMs_.load(std::memory_order_relaxed); }
    double avgPollMs() const { return avgPollMs_.load(std::memory_order_relaxed); }
    double fps() const;
    double lastInputLatencyMs() const { return lastLatencyMs_.load(std::memory_order_relaxed); }
    double maxInputLatencyMs() const { return maxLatencyMs_.load(std::memory_order_relaxed); }
    unsigned long long frameCount() const { return frames_.load(std::memory_order_relaxed); }

private:
    static void smooth(std::atomic<double>& slot, double sample);

    std::atomic<double> avgFrameMs_{0.0};
    std::atomic<double> avgComposeMs_{0.0};
    std::atomic<double> avgPollMs_{0.0};
    std::atomic<double> lastLatencyMs_{0.0};
    std::atomic<double> maxLatencyMs_{0.0};
    std::atomic<unsigned long long> frames_{0};
};

}  // namespace mc
