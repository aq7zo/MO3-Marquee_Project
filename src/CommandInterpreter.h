#pragma once

#include <atomic>
#include <string>

#include "CommandParser.h"
#include "ConsoleState.h"
#include "Process.h"
#include "Scheduler.h"

namespace mc {

// Executes parsed commands against the shared state.
//
// Rate changes are pushed straight into the owning Process rather than being
// picked up on its next tick, so `set_speed 50` after `set_speed 60000` takes
// effect immediately instead of a minute later.
class CommandInterpreter {
public:
    struct ProcessHandles {
        Process* marquee = nullptr;
        Process* display = nullptr;
        Process* keyboard = nullptr;
    };

    explicit CommandInterpreter(ConsoleState& state);

    // Attached after construction because the processes do not exist yet when
    // the interpreter is created; this also lets the interpreter outlive the
    // Scheduler so joining threads can never touch a destroyed interpreter.
    void attach(Scheduler* scheduler, ProcessHandles handles);

    void execute(const std::wstring& line);
    void printWelcome(bool configFileFound);

private:
    void handleHelp();
    void handleStartMarquee();
    void handleStopMarquee();
    void handleSetText(const ParsedCommand& cmd);
    void handleStatus();
    void handleExit();
    void handleUnknown(const ParsedCommand& cmd);

    // Shared by set_speed, set_refresh and set_polling_rate so all three
    // validate and report identically.
    void applyRate(const ParsedCommand& cmd,
                   const wchar_t* label,
                   int minMs,
                   int maxMs,
                   std::atomic<int>& slot,
                   Process* target);

    ConsoleState& state_;
    Scheduler* scheduler_ = nullptr;
    ProcessHandles handles_;
};

}  // namespace mc
