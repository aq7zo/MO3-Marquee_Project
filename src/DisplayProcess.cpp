#include "DisplayProcess.h"

namespace mc {

namespace {
double msSince(std::chrono::steady_clock::time_point from,
               std::chrono::steady_clock::time_point to) {
    return std::chrono::duration<double, std::milli>(to - from).count();
}
}  // namespace

DisplayProcess::DisplayProcess(ConsoleState& state, ConsoleScreen& screen, const Renderer& renderer)
    : Process("display", state.refreshMs.load()),
      state_(state),
      screen_(screen),
      renderer_(renderer),
      frame_(screen.width(), screen.height()) {}

void DisplayProcess::tick() {
    const auto start = std::chrono::steady_clock::now();

    renderer_.compose(frame_, state_);
    const auto composed = std::chrono::steady_clock::now();

    screen_.present(frame_);

    // Closes the input-latency measurement: any keystroke recorded since the
    // last frame has now actually reached the screen.
    state_.markFramePresented();

    if (haveLastFrame_) {
        state_.metrics.recordFrame(msSince(lastFrame_, start), msSince(start, composed));
    }
    lastFrame_ = start;
    haveLastFrame_ = true;
}

}  // namespace mc
