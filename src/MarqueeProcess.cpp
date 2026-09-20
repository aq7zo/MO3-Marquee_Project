#include "MarqueeProcess.h"

namespace mc {

MarqueeProcess::MarqueeProcess(ConsoleState& state)
    : Process("marquee", state.marqueeSpeedMs.load()), state_(state) {}

void MarqueeProcess::tick() {
    // stop_marquee freezes the position but leaves the text on screen, as the
    // specification describes: it stops the animation, not the marquee.
    if (state_.marqueeRunning.load()) state_.tickMarquee();
}

}  // namespace mc
