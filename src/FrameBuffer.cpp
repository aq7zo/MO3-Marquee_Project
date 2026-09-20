#include "FrameBuffer.h"

#include <algorithm>

namespace mc {

namespace {
// Box-drawing glyphs, written as escapes rather than literal characters so the
// source file's encoding can never change how they compile.
constexpr wchar_t kTopLeft = L'\u250C';
constexpr wchar_t kTopRight = L'\u2510';
constexpr wchar_t kBottomLeft = L'\u2514';
constexpr wchar_t kBottomRight = L'\u2518';
constexpr wchar_t kHorizontal = L'\u2500';
constexpr wchar_t kVertical = L'\u2502';
}  // namespace

FrameBuffer::FrameBuffer(int width, int height)
    : width_(std::max(0, width)), height_(std::max(0, height)) {
    cells_.resize(static_cast<size_t>(width_) * static_cast<size_t>(height_));
    clear();
}

void FrameBuffer::resize(int width, int height) {
    width_ = std::max(0, width);
    height_ = std::max(0, height);
    cells_.assign(static_cast<size_t>(width_) * static_cast<size_t>(height_), CHAR_INFO{});
    clear();
}

void FrameBuffer::clear(WORD attr) {
    for (CHAR_INFO& cell : cells_) {
        cell.Char.UnicodeChar = L' ';
        cell.Attributes = attr;
    }
}

void FrameBuffer::putChar(int x, int y, wchar_t ch, WORD attr) {
    if (!inBounds(x, y)) return;
    CHAR_INFO& cell = cells_[static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x)];
    cell.Char.UnicodeChar = ch;
    cell.Attributes = attr;
}

void FrameBuffer::drawText(int x, int y, std::wstring_view text, WORD attr) {
    if (y < 0 || y >= height_) return;
    for (size_t i = 0; i < text.size(); ++i) {
        const int cx = x + static_cast<int>(i);
        if (cx < 0) continue;
        if (cx >= width_) break;
        putChar(cx, y, text[i], attr);
    }
}

void FrameBuffer::drawTextClipped(int x, int y, int maxWidth, std::wstring_view text, WORD attr) {
    if (maxWidth <= 0) return;
    if (text.size() > static_cast<size_t>(maxWidth)) {
        text = text.substr(0, static_cast<size_t>(maxWidth));
    }
    drawText(x, y, text, attr);
}

void FrameBuffer::drawHLine(int x, int y, int length, wchar_t ch, WORD attr) {
    for (int i = 0; i < length; ++i) putChar(x + i, y, ch, attr);
}

void FrameBuffer::drawVLine(int x, int y, int length, wchar_t ch, WORD attr) {
    for (int i = 0; i < length; ++i) putChar(x, y + i, ch, attr);
}

void FrameBuffer::fillRect(int x, int y, int w, int h, wchar_t ch, WORD attr) {
    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) putChar(x + col, y + row, ch, attr);
    }
}

void FrameBuffer::drawBox(int x, int y, int w, int h, WORD attr) {
    if (w < 2 || h < 2) return;
    drawHLine(x + 1, y, w - 2, kHorizontal, attr);
    drawHLine(x + 1, y + h - 1, w - 2, kHorizontal, attr);
    drawVLine(x, y + 1, h - 2, kVertical, attr);
    drawVLine(x + w - 1, y + 1, h - 2, kVertical, attr);
    putChar(x, y, kTopLeft, attr);
    putChar(x + w - 1, y, kTopRight, attr);
    putChar(x, y + h - 1, kBottomLeft, attr);
    putChar(x + w - 1, y + h - 1, kBottomRight, attr);
}

}  // namespace mc
