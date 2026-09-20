#pragma once

#include <chrono>

#include "ConsoleScreen.h"
#include "ConsoleState.h"
#include "FrameBuffer.h"
#include "Process.h"
#include "Renderer.h"

namespace mc {

// Composes and presents one frame per tick.
//
// The frame is built into a reusable back buffer and handed to the console in a
// single WriteConsoleOutputW call, so no partially drawn frame is ever visible.
// This is where the refresh rate half of the refresh-versus-polling trade-off
// lives.
class DisplayProcess : public Process {
public:
    DisplayProcess(ConsoleState& state, ConsoleScreen& screen, const Renderer& renderer);

protected:
    void tick() override;

private:
    ConsoleState& state_;
    ConsoleScreen& screen_;
    const Renderer& renderer_;
    FrameBuffer frame_;
    std::chrono::steady_clock::time_point lastFrame_{};
    bool haveLastFrame_ = false;
};

}  // namespace mc
