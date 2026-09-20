#include "ConsoleScreen.h"

#include <algorithm>

namespace mc {

namespace {
constexpr SHORT kMinDimension = 1;

SHORT clampToShort(int value) {
    return static_cast<SHORT>(std::clamp(value, static_cast<int>(kMinDimension), 32767));
}
}  // namespace

ConsoleScreen::ConsoleScreen(int width, int height, const std::wstring& title) {
    hOut_ = GetStdHandle(STD_OUTPUT_HANDLE);
    hIn_ = GetStdHandle(STD_INPUT_HANDLE);

    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (hOut_ == INVALID_HANDLE_VALUE || hIn_ == INVALID_HANDLE_VALUE ||
        !GetConsoleScreenBufferInfo(hOut_, &info)) {
        // No real console attached; leave attached_ false and touch nothing.
        return;
    }
    attached_ = true;

    origBufferSize_ = info.dwSize;
    origWindow_ = info.srWindow;
    haveOrigInMode_ = GetConsoleMode(hIn_, &origInMode_) != FALSE;
    haveOrigOutMode_ = GetConsoleMode(hOut_, &origOutMode_) != FALSE;
    GetConsoleCursorInfo(hOut_, &origCursor_);

    wchar_t titleBuffer[512] = {};
    if (GetConsoleTitleW(titleBuffer, static_cast<DWORD>(std::size(titleBuffer))) > 0) {
        origTitle_.assign(titleBuffer);
    }

    SetConsoleTitleW(title.c_str());
    applyRawInputMode();
    lockWindowSize();
    applySize(width, height);

    CONSOLE_CURSOR_INFO cursor = origCursor_;
    cursor.bVisible = FALSE;
    SetConsoleCursorInfo(hOut_, &cursor);

    clear();
}

ConsoleScreen::~ConsoleScreen() { restore(); }

void ConsoleScreen::applyRawInputMode() {
    // Clear LINE_INPUT and ECHO_INPUT so keystrokes arrive one at a time and the
    // console does not echo them itself - we draw the prompt line ourselves.
    //
    // PROCESSED_INPUT is cleared too, so Ctrl+C arrives as an ordinary key event
    // and can be handled gracefully instead of killing the process mid-frame.
    //
    // QUICK_EDIT_MODE is disabled because a stray click would otherwise select
    // text and block the render thread's console writes until dismissed. Turning
    // it off requires setting EXTENDED_FLAGS in the same call.
    DWORD mode = ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT;
    SetConsoleMode(hIn_, mode);
}

void ConsoleScreen::lockWindowSize() {
    // The layout is fixed at console_width x console_height, so a drag on the
    // border or a click on Maximize can only clip the frame or leave dead space:
    // conhost resizes its window without ever telling the buffer to follow.
    // Dropping WS_THICKFRAME and WS_MAXIMIZEBOX removes both gestures - Windows
    // greys the matching Size/Maximize entries in the system menu by itself.
    //
    // Done before applySize so the window is measured with its final frame
    // style; the style is put back in restore() because `make run` locks the
    // user's own console window, not a throwaway one.
    hWnd_ = GetConsoleWindow();
    if (!hWnd_) return;

    origStyle_ = GetWindowLongPtrW(hWnd_, GWL_STYLE);
    if (origStyle_ == 0) return;
    haveOrigStyle_ = true;
    SetWindowLongPtrW(hWnd_, GWL_STYLE, origStyle_ & ~(WS_THICKFRAME | WS_MAXIMIZEBOX));
}

void ConsoleScreen::applySize(int width, int height) {
    const COORD largest = GetLargestConsoleWindowSize(hOut_);
    if (largest.X > 0) width = std::min(width, static_cast<int>(largest.X));
    if (largest.Y > 0) height = std::min(height, static_cast<int>(largest.Y));

    const SHORT w = clampToShort(width);
    const SHORT h = clampToShort(height);

    // Shrink the window before resizing the buffer, then grow the window to fit.
    // The buffer can never be smaller than the window, so this order is the only
    // one that works when growing or shrinking.
    SMALL_RECT minimal{0, 0, 1, 1};
    SetConsoleWindowInfo(hOut_, TRUE, &minimal);
    SetConsoleScreenBufferSize(hOut_, COORD{w, h});

    SMALL_RECT target{0, 0, static_cast<SHORT>(w - 1), static_cast<SHORT>(h - 1)};
    SetConsoleWindowInfo(hOut_, TRUE, &target);

    // Trust what the console actually gave us rather than what we asked for.
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (GetConsoleScreenBufferInfo(hOut_, &info)) {
        width_ = info.dwSize.X;
        height_ = info.dwSize.Y;
    } else {
        width_ = w;
        height_ = h;
    }
}

void ConsoleScreen::clear() {
    if (!attached_) return;

    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!GetConsoleScreenBufferInfo(hOut_, &info)) return;

    const DWORD cells = static_cast<DWORD>(info.dwSize.X) * static_cast<DWORD>(info.dwSize.Y);
    DWORD written = 0;
    FillConsoleOutputCharacterW(hOut_, L' ', cells, COORD{0, 0}, &written);
    FillConsoleOutputAttribute(hOut_, info.wAttributes, cells, COORD{0, 0}, &written);
    SetConsoleCursorPosition(hOut_, COORD{0, 0});
}

void ConsoleScreen::present(const FrameBuffer& fb) {
    if (!attached_ || fb.width() <= 0 || fb.height() <= 0) return;

    const SHORT w = static_cast<SHORT>(std::min(fb.width(), width_));
    const SHORT h = static_cast<SHORT>(std::min(fb.height(), height_));

    SMALL_RECT region{0, 0, static_cast<SHORT>(w - 1), static_cast<SHORT>(h - 1)};
    WriteConsoleOutputW(hOut_,
                        fb.data(),
                        COORD{static_cast<SHORT>(fb.width()), static_cast<SHORT>(fb.height())},
                        COORD{0, 0},
                        &region);
}

void ConsoleScreen::restore() noexcept {
    if (!attached_ || restored_) return;
    restored_ = true;

    SetConsoleCursorInfo(hOut_, &origCursor_);
    if (haveOrigInMode_) SetConsoleMode(hIn_, origInMode_);
    if (haveOrigOutMode_) SetConsoleMode(hOut_, origOutMode_);

    SMALL_RECT minimal{0, 0, 1, 1};
    SetConsoleWindowInfo(hOut_, TRUE, &minimal);
    SetConsoleScreenBufferSize(hOut_, origBufferSize_);
    SetConsoleWindowInfo(hOut_, TRUE, &origWindow_);

    if (haveOrigStyle_) SetWindowLongPtrW(hWnd_, GWL_STYLE, origStyle_);

    if (!origTitle_.empty()) SetConsoleTitleW(origTitle_.c_str());

    // Last, so the restored buffer is what gets blanked.
    clear();
}

}  // namespace mc
