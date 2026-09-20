#include "MarqueeState.h"

#include <algorithm>

namespace mc {

namespace {

// Moves one axis by dir and reflects at either end. The object occupies
// `extent` cells inside `limit` cells, so the last valid position is
// limit - extent.
//
// When the object cannot fit (extent >= limit) or the axis is degenerate, the
// position is pinned to 0 and the axis simply stops moving - that keeps an
// oversized marquee string or a zero-height zone from producing out-of-range
// coordinates instead of crashing.
void stepAxis(int& pos, int& dir, int extent, int limit) {
    if (limit <= 0 || extent <= 0 || extent >= limit) {
        pos = 0;
        return;
    }

    const int maxPos = limit - extent;
    pos += dir;
    if (pos < 0) {
        pos = 0;
        dir = 1;
    } else if (pos > maxPos) {
        pos = maxPos;
        dir = -1;
    }
}

}  // namespace

void MarqueeState::setBounds(int zoneWidth, int zoneHeight) {
    zoneWidth_ = std::max(0, zoneWidth);
    zoneHeight_ = std::max(0, zoneHeight);
    clampPosition();
}

void MarqueeState::setText(std::wstring text) {
    text_ = std::move(text);
    clampPosition();
}

void MarqueeState::tick() {
    stepAxis(x_, dx_, textWidth(), zoneWidth_);
    stepAxis(y_, dy_, 1, zoneHeight_);
}

void MarqueeState::clampPosition() {
    const int maxX = zoneWidth_ - textWidth();
    x_ = (maxX <= 0) ? 0 : std::clamp(x_, 0, maxX);

    const int maxY = zoneHeight_ - 1;
    y_ = (maxY <= 0) ? 0 : std::clamp(y_, 0, maxY);
}

}  // namespace mc
