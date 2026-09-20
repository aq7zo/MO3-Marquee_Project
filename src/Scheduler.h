#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Process.h"

namespace mc {

// Owns the lifecycle of every Process in the emulator.
//
// This is the "scheduler implementation" the technical report asks about: it
// admits processes, starts them together, and on shutdown signals every one of
// them before joining any of them. Signalling first matters - stopping and
// joining one at a time would serialise the shutdown and let a long-period
// process hold everything else up.
class Scheduler {
public:
    // Returns a borrowed pointer so callers can retune a specific process at
    // runtime (set_speed, set_refresh, set_polling_rate). The Scheduler retains
    // ownership.
    Process* add(std::unique_ptr<Process> process);

    void startAll();
    void stopAll();

    struct ProcessStatus {
        std::string name;
        int periodMs;
    };
    std::vector<ProcessStatus> status() const;

private:
    std::vector<std::unique_ptr<Process>> processes_;
};

}  // namespace mc
