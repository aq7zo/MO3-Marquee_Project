#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include "Config.h"
#include "MarqueeState.h"
#include "Metrics.h"

namespace mc {

// Everything shared between the four processes.
//
// All locking is internal - callers cannot forget to take a lock, and no lock
// is ever held across a Win32 call, because every accessor either copies out a
// snapshot or mutates and returns immediately.
//
// Separate mutexes keep the render path from ever blocking on keystroke
// handling, which is the whole point of decoupling the rates.
class ConsoleState {
public:
    explicit ConsoleState(const Config& config);

    // ---- lifecycle -------------------------------------------------------
    std::atomic<bool> shutdownRequested{false};
    void requestShutdown();

    // Build version, copied from the config at construction. Const, so `status`
    // can read it without a lock.
    const std::wstring version;

    // ---- runtime-tunable rates (the "parameters" the quiz lets you change) --
    std::atomic<int> marqueeSpeedMs;
    std::atomic<int> refreshMs;
    std::atomic<int> pollingMs;
    std::atomic<bool> marqueeRunning;

    // ---- marquee ---------------------------------------------------------
    struct MarqueeSnapshot {
        std::wstring text;
        int x = 0;
        int y = 0;
        bool overflows = false;
    };

    void setMarqueeBounds(int width, int height);
    void setMarqueeText(std::wstring text);
    std::wstring marqueeText() const;
    void tickMarquee();
    MarqueeSnapshot marqueeSnapshot() const;

    // ---- input line ------------------------------------------------------
    // The caret can sit anywhere in the line, so the renderer needs the text
    // and the caret index together: reading them through two accessors could
    // interleave with a keystroke and draw the caret past the end of the text.
    struct InputSnapshot {
        std::wstring text;
        int cursor = 0;
    };

    void inputAppend(wchar_t ch);
    void inputBackspace();
    void inputClear();
    InputSnapshot inputSnapshot() const;

    // Named after what the key does rather than the key itself - Home and
    // Ctrl+A are the same motion, and only KeyboardProcess needs to know that.
    enum class CursorMove { Left, Right, WordLeft, WordRight, Home, End };
    void inputMoveCursor(CursorMove move);

    // Ctrl+U and Ctrl+K.
    void inputKillToStart();
    void inputKillToEnd();

    // Up/Ctrl+P walk towards older entries, Down/Ctrl+N back towards the line
    // that was being typed when the walk started.
    enum class HistoryStep { Prev, Next };
    void inputRecallHistory(HistoryStep step);

    // Moves the current input line onto the command queue and clears it.
    // Returns false when the line was blank, so blank Enter presses are ignored
    // rather than producing an error for every stray keypress.
    bool submitInputLine();

    // ---- command queue ---------------------------------------------------
    void pushCommand(std::wstring line);
    bool waitAndPopCommand(std::wstring& out, std::chrono::milliseconds timeout);

    // ---- output history --------------------------------------------------
    void addOutput(std::wstring line);
    void addOutputLines(const std::vector<std::wstring>& lines);
    void clearOutput();
    std::vector<std::wstring> outputTail(size_t count) const;

    // ---- input latency instrumentation -----------------------------------
    // The keyboard process stamps the arrival of a keystroke; the display
    // process closes the loop once that keystroke has actually been drawn.
    void markKeyReceived();
    void markFramePresented();

    Metrics metrics;

private:
    mutable std::mutex marqueeMutex_;
    MarqueeState marquee_;

    mutable std::mutex inputMutex_;
    std::wstring inputLine_;
    size_t inputCursor_ = 0;

    // historyIndex_ == history_.size() means "not browsing": the line being
    // edited is a new one. historyDraft_ holds that new line while a walk
    // through older entries is in progress, so Down can hand it back.
    std::vector<std::wstring> history_;
    size_t historyIndex_ = 0;
    std::wstring historyDraft_;

    mutable std::mutex outputMutex_;
    std::deque<std::wstring> output_;

    mutable std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::deque<std::wstring> commandQueue_;

    // 0 means "no keystroke awaiting display".
    std::atomic<long long> pendingKeyTicks_{0};

    static constexpr size_t kMaxOutputLines = 500;
    static constexpr size_t kMaxHistory = 100;
};

}  // namespace mc
