#include "Metrics.h"

namespace mc {

namespace {
constexpr double kSmoothing = 0.1;  // weight of the newest sample
}

void Metrics::smooth(std::atomic<double>& slot, double sample) {
    const double previous = slot.load(std::memory_order_relaxed);
    const double next = (previous <= 0.0) ? sample : previous * (1.0 - kSmoothing) + sample * kSmoothing;
    slot.store(next, std::memory_order_relaxed);
}

void Metrics::recordFrame(double frameMs, double composeMs) {
    smooth(avgFrameMs_, frameMs);
    smooth(avgComposeMs_, composeMs);
    frames_.fetch_add(1, std::memory_order_relaxed);
}

void Metrics::recordPoll(double intervalMs) { smooth(avgPollMs_, intervalMs); }

void Metrics::recordInputLatency(double ms) {
    lastLatencyMs_.store(ms, std::memory_order_relaxed);
    if (ms > maxLatencyMs_.load(std::memory_order_relaxed)) {
        maxLatencyMs_.store(ms, std::memory_order_relaxed);
    }
}

void Metrics::resetPeaks() { maxLatencyMs_.store(0.0, std::memory_order_relaxed); }

double Metrics::fps() const {
    const double ms = avgFrameMs_.load(std::memory_order_relaxed);
    return (ms > 0.0) ? 1000.0 / ms : 0.0;
}

}  // namespace mc
