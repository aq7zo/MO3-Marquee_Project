#include "Scheduler.h"

namespace mc {

Process* Scheduler::add(std::unique_ptr<Process> process) {
    Process* borrowed = process.get();
    processes_.push_back(std::move(process));
    return borrowed;
}

void Scheduler::startAll() {
    for (auto& process : processes_) process->start();
}

void Scheduler::stopAll() {
    // Signal everything first, then join. Interleaving stop and join would make
    // shutdown take as long as the sum of the periods instead of the longest.
    for (auto& process : processes_) process->requestStop();
    for (auto& process : processes_) process->join();
}

std::vector<Scheduler::ProcessStatus> Scheduler::status() const {
    std::vector<ProcessStatus> out;
    out.reserve(processes_.size());
    for (const auto& process : processes_) {
        out.push_back(ProcessStatus{process->name(), process->periodMs()});
    }
    return out;
}

}  // namespace mc
