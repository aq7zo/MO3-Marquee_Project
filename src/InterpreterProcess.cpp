#include "InterpreterProcess.h"

#include <chrono>

namespace mc {

namespace {
constexpr int kQueueWaitMs = 50;
}

InterpreterProcess::InterpreterProcess(ConsoleState& state, CommandInterpreter& interpreter)
    : Process("interpreter", kQueueWaitMs), state_(state), interpreter_(interpreter) {}

void InterpreterProcess::tick() {
    std::wstring line;
    if (state_.waitAndPopCommand(line, std::chrono::milliseconds(kQueueWaitMs))) {
        interpreter_.execute(line);
    }
}

}  // namespace mc
