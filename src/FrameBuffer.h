#pragma once

#include <windows.h>

#include <string_view>
#include <vector>

namespace mc {

// Console attribute helpers, named so call sites read clearly.
namespace color {
constexpr WORD kGray = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
constexpr WORD kWhite = kGray | FOREGROUND_INTENSITY;
constexpr WORD kDarkGray = FOREGROUND_INTENSITY;
constexpr WORD kCyan = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
constexpr WORD kGreen = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr WORD kYellow = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr WORD kRed = FOREGROUND_RED | FOREGROUND_INTENSITY;
constexpr WORD kMagenta = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
}  // namespace color

// A CHAR_INFO grid composed entirely off-screen, then handed to
// ConsoleScreen::present() in one WriteConsoleOutputW call. Composing here
// rather than writing to the console incrementally is what makes rendering
// tear-free: the console never shows a half-built frame.
//
// Every draw call clips silently. Out-of-bounds coordinates are normal (the
// marquee routinely straddles an edge), not an error.
class FrameBuffer {
public:
    FrameBuffer(int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }
    const CHAR_INFO* data() const { return cells_.data(); }

    void resize(int width, int height);
    void clear(WORD attr = color::kGray);

    void putChar(int x, int y, wchar_t ch, WORD attr);
    void drawText(int x, int y, std::wstring_view text, WORD attr);

    // Draws at most maxWidth cells starting at x, clipping the tail.
    void drawTextClipped(int x, int y, int maxWidth, std::wstring_view text, WORD attr);

    void drawHLine(int x, int y, int length, wchar_t ch, WORD attr);
    void drawVLine(int x, int y, int length, wchar_t ch, WORD attr);
    void fillRect(int x, int y, int w, int h, wchar_t ch, WORD attr);
    void drawBox(int x, int y, int w, int h, WORD attr);

private:
    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < width_ && y < height_; }

    int width_;
    int height_;
    std::vector<CHAR_INFO> cells_;
};

}  // namespace mc
