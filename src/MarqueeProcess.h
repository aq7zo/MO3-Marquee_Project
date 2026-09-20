#pragma once

#include "ConsoleState.h"
#include "Process.h"

namespace mc {

// Advances the marquee one step per tick. Its period is the value set by
// set_speed - deliberately independent of the display refresh, so the animation
// can be slowed to a crawl without the console becoming unresponsive.
class MarqueeProcess : public Process {
public:
    explicit MarqueeProcess(ConsoleState& state);

protected:
    void tick() override;

private:
    ConsoleState& state_;
};

}  // namespace mc
