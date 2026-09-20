#pragma once

#include <string>

namespace mc {

// Bouncing-marquee geometry. Deliberately free of Win32 and of any I/O so the
// bounce behaviour can be unit-tested without a console attached.
//
// Position is in cells local to the marquee zone: (0,0) is the zone's top-left
// interior cell.
class MarqueeState {
public:
    MarqueeState() = default;

    void setBounds(int zoneWidth, int zoneHeight);
    void setText(std::wstring text);

    // Advances one animation step and reflects off any edge it reaches.
    void tick();

    const std::wstring& text() const { return text_; }
    int x() const { return x_; }
    int y() const { return y_; }
    int dx() const { return dx_; }
    int dy() const { return dy_; }
    int zoneWidth() const { return zoneWidth_; }
    int zoneHeight() const { return zoneHeight_; }

    // True when the text cannot fit horizontally, in which case it is pinned to
    // the left edge and only moves vertically.
    bool textOverflows() const { return textWidth() >= zoneWidth_; }

private:
    int textWidth() const { return static_cast<int>(text_.size()); }
    void clampPosition();

    std::wstring text_;
    int zoneWidth_ = 0;
    int zoneHeight_ = 0;
    int x_ = 0;
    int y_ = 0;
    int dx_ = 1;
    int dy_ = 1;
};

}  // namespace mc
