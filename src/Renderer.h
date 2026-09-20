#pragma once

#include <string>
#include <vector>

#include "Config.h"
#include "ConsoleState.h"
#include "FrameBuffer.h"

namespace mc {

// Where each zone of the screen lives. Derived once from the console size and
// the configured marquee height, then clamped so the zones can never overlap or
// run off the bottom on a small console.
struct Layout {
    int width = 0;
    int height = 0;
    int headerHeight = 0;
    int marqueeTop = 0;
    int marqueeHeight = 0;
    int outputTop = 0;
    int outputHeight = 0;
    int promptRow = 0;
    int hintRow = 0;

    int marqueeInteriorWidth() const { return (width > 2) ? width - 2 : 0; }
    int marqueeInteriorHeight() const { return (marqueeHeight > 2) ? marqueeHeight - 2 : 0; }
    int outputInteriorHeight() const { return (outputHeight > 2) ? outputHeight - 2 : 0; }
};

// Composes a complete frame into a FrameBuffer. Holds no console handles and
// performs no I/O, so the display process can be reasoned about separately from
// the Win32 layer.
class Renderer {
public:
    Renderer(const Config& config, int consoleWidth, int consoleHeight);

    const Layout& layout() const { return layout_; }

    void compose(FrameBuffer& fb, const ConsoleState& state) const;

private:
    void drawHeader(FrameBuffer& fb) const;
    void drawMarquee(FrameBuffer& fb, const ConsoleState& state) const;
    void drawOutput(FrameBuffer& fb, const ConsoleState& state) const;
    void drawPrompt(FrameBuffer& fb, const ConsoleState& state) const;

    Config config_;
    Layout layout_;
    std::vector<std::wstring> banner_;
};

}  // namespace mc
