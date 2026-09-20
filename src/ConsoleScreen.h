#pragma once

#include <windows.h>

#include <string>

#include "FrameBuffer.h"

namespace mc {

// RAII owner of console state.
//
// Captures the original input/output modes, cursor visibility, buffer size and
// title on construction and restores them on destruction - including while the
// stack unwinds - so the terminal is never left in raw mode with a hidden
// cursor, even if something throws.
//
// attached() is false when the process has no real console (for example when
// launched from a pty such as MinTTY). Callers must check it before rendering
// rather than letting the Win32 calls fail one by one.
class ConsoleScreen {
public:
    ConsoleScreen(int width, int height, const std::wstring& title);
    ~ConsoleScreen();

    ConsoleScreen(const ConsoleScreen&) = delete;
    ConsoleScreen& operator=(const ConsoleScreen&) = delete;

    bool attached() const { return attached_; }
    int width() const { return width_; }
    int height() const { return height_; }
    HANDLE inputHandle() const { return hIn_; }

    // One WriteConsoleOutputW of the entire frame: the console can never show a
    // partially built frame, which is what eliminates tearing.
    void present(const FrameBuffer& fb);

    // Blanks the whole buffer and homes the cursor. Called on construction so a
    // run never starts under leftover shell output, and again on destruction so
    // `exit` leaves the terminal clean instead of showing the last frame.
    void clear();

private:
    void applyRawInputMode();
    void lockWindowSize();
    void applySize(int width, int height);
    void restore() noexcept;

    HANDLE hOut_ = INVALID_HANDLE_VALUE;
    HANDLE hIn_ = INVALID_HANDLE_VALUE;
    bool attached_ = false;
    bool restored_ = false;

    int width_ = 0;
    int height_ = 0;

    DWORD origInMode_ = 0;
    DWORD origOutMode_ = 0;
    bool haveOrigInMode_ = false;
    bool haveOrigOutMode_ = false;
    CONSOLE_CURSOR_INFO origCursor_{};
    COORD origBufferSize_{};
    SMALL_RECT origWindow_{};
    std::wstring origTitle_;

    HWND hWnd_ = nullptr;
    LONG_PTR origStyle_ = 0;
    bool haveOrigStyle_ = false;
};

}  // namespace mc
