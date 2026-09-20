#pragma once

#include "CommandInterpreter.h"
#include "ConsoleState.h"
#include "Process.h"

namespace mc {

// Drains the command queue. Unlike the other three this process is
// event-driven: it blocks on the queue's condition variable instead of waking
// on a fixed period, so an idle console costs nothing at all.
//
// The bounded wait keeps shutdown prompt even if a notify is ever missed.
class InterpreterProcess : public Process {
public:
    InterpreterProcess(ConsoleState& state, CommandInterpreter& interpreter);

protected:
    void tick() override;
    bool blocksInTick() const override { return true; }

private:
    ConsoleState& state_;
    CommandInterpreter& interpreter_;
};

}  // namespace mc
