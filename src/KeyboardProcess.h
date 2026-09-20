#pragma once

#include <windows.h>

#include <chrono>

#include "ConsoleState.h"
#include "Process.h"

namespace mc {

// Polls the console input buffer once per tick and edits the shared input line.
//
// This is the polling rate half of the refresh-versus-polling trade-off: too
// slow and keystrokes visibly lag behind typing, too fast and the thread burns
// CPU for no benefit.
class KeyboardProcess : public Process {
public:
    KeyboardProcess(ConsoleState& state, HANDLE input);

protected:
    void tick() override;

private:
    void handleKeyEvent(const KEY_EVENT_RECORD& key);

    ConsoleState& state_;
    HANDLE input_;
    std::chrono::steady_clock::time_point lastPoll_{};
    bool havePolled_ = false;
};

}  // namespace mc
