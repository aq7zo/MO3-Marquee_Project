#include "ConsoleState.h"

#include <algorithm>

namespace mc {

namespace {
long long nowTicks() {
    return std::chrono::steady_clock::now().time_since_epoch().count();
}

// Word motion treats any run of non-blanks as a word, so a path or a quoted
// argument jumps in one go rather than stopping on every punctuation mark.
bool isWordChar(wchar_t c) { return c != L' ' && c != L'\t'; }

// Both land on the start of a word, which makes the two motions exact
// inverses - Ctrl+Left then Ctrl+Right returns the caret where it was.
size_t wordLeft(const std::wstring& s, size_t i) {
    while (i > 0 && !isWordChar(s[i - 1])) --i;
    while (i > 0 && isWordChar(s[i - 1])) --i;
    return i;
}

size_t wordRight(const std::wstring& s, size_t i) {
    while (i < s.size() && isWordChar(s[i])) ++i;
    while (i < s.size() && !isWordChar(s[i])) ++i;
    return i;
}

double ticksToMs(long long ticks) {
    using Period = std::chrono::steady_clock::period;
    return static_cast<double>(ticks) * 1000.0 * Period::num / Period::den;
}
}  // namespace

ConsoleState::ConsoleState(const Config& config)
    : version(config.version),
      marqueeSpeedMs(config.marqueeSpeedMs),
      refreshMs(config.refreshMs),
      pollingMs(config.pollingMs),
      marqueeRunning(config.autoStartMarquee) {
    marquee_.setText(config.marqueeText);
}

void ConsoleState::requestShutdown() {
    {
        // Taking the queue lock closes the window where the interpreter has
        // evaluated its wait predicate but not yet slept, which would otherwise
        // drop this notify and delay shutdown by a full queue timeout.
        std::lock_guard<std::mutex> lock(queueMutex_);
        shutdownRequested.store(true);
    }
    queueCv_.notify_all();
}

// ---- marquee -------------------------------------------------------------

void ConsoleState::setMarqueeBounds(int width, int height) {
    std::lock_guard<std::mutex> lock(marqueeMutex_);
    marquee_.setBounds(width, height);
}

void ConsoleState::setMarqueeText(std::wstring text) {
    std::lock_guard<std::mutex> lock(marqueeMutex_);
    marquee_.setText(std::move(text));
}

std::wstring ConsoleState::marqueeText() const {
    std::lock_guard<std::mutex> lock(marqueeMutex_);
    return marquee_.text();
}

void ConsoleState::tickMarquee() {
    std::lock_guard<std::mutex> lock(marqueeMutex_);
    marquee_.tick();
}

ConsoleState::MarqueeSnapshot ConsoleState::marqueeSnapshot() const {
    std::lock_guard<std::mutex> lock(marqueeMutex_);
    MarqueeSnapshot snap;
    snap.text = marquee_.text();
    snap.x = marquee_.x();
    snap.y = marquee_.y();
    snap.overflows = marquee_.textOverflows();
    return snap;
}

// ---- input line ----------------------------------------------------------

void ConsoleState::inputAppend(wchar_t ch) {
    std::lock_guard<std::mutex> lock(inputMutex_);
    if (static_cast<int>(inputLine_.size()) >= limits::kMaxInputLength) return;
    inputLine_.insert(inputLine_.begin() + static_cast<ptrdiff_t>(inputCursor_), ch);
    ++inputCursor_;
}

void ConsoleState::inputBackspace() {
    std::lock_guard<std::mutex> lock(inputMutex_);
    if (inputCursor_ == 0) return;
    inputLine_.erase(inputCursor_ - 1, 1);
    --inputCursor_;
}

void ConsoleState::inputClear() {
    std::lock_guard<std::mutex> lock(inputMutex_);
    inputLine_.clear();
    inputCursor_ = 0;
}

void ConsoleState::inputMoveCursor(CursorMove move) {
    std::lock_guard<std::mutex> lock(inputMutex_);
    switch (move) {
        case CursorMove::Left:
            if (inputCursor_ > 0) --inputCursor_;
            break;
        case CursorMove::Right:
            if (inputCursor_ < inputLine_.size()) ++inputCursor_;
            break;
        case CursorMove::WordLeft:
            inputCursor_ = wordLeft(inputLine_, inputCursor_);
            break;
        case CursorMove::WordRight:
            inputCursor_ = wordRight(inputLine_, inputCursor_);
            break;
        case CursorMove::Home:
            inputCursor_ = 0;
            break;
        case CursorMove::End:
            inputCursor_ = inputLine_.size();
            break;
    }
}

void ConsoleState::inputKillToStart() {
    std::lock_guard<std::mutex> lock(inputMutex_);
    inputLine_.erase(0, inputCursor_);
    inputCursor_ = 0;
}

void ConsoleState::inputKillToEnd() {
    std::lock_guard<std::mutex> lock(inputMutex_);
    inputLine_.erase(inputCursor_);
}

void ConsoleState::inputRecallHistory(HistoryStep step) {
    std::lock_guard<std::mutex> lock(inputMutex_);
    if (history_.empty()) return;

    if (step == HistoryStep::Prev) {
        if (historyIndex_ == 0) return;  // already showing the oldest entry
        if (historyIndex_ == history_.size()) historyDraft_ = inputLine_;
        --historyIndex_;
        inputLine_ = history_[historyIndex_];
    } else {
        if (historyIndex_ >= history_.size()) return;  // not browsing
        ++historyIndex_;
        inputLine_ = (historyIndex_ == history_.size()) ? historyDraft_
                                                       : history_[historyIndex_];
    }
    inputCursor_ = inputLine_.size();
}

ConsoleState::InputSnapshot ConsoleState::inputSnapshot() const {
    std::lock_guard<std::mutex> lock(inputMutex_);
    return InputSnapshot{inputLine_, static_cast<int>(inputCursor_)};
}

bool ConsoleState::submitInputLine() {
    std::wstring line;
    {
        std::lock_guard<std::mutex> lock(inputMutex_);
        line.swap(inputLine_);
        inputCursor_ = 0;
        historyDraft_.clear();

        // Consecutive duplicates are dropped so holding Enter on one command
        // does not bury everything older behind repeats of it.
        if (line.find_first_not_of(L" \t") != std::wstring::npos &&
            (history_.empty() || history_.back() != line)) {
            history_.push_back(line);
            if (history_.size() > kMaxHistory) history_.erase(history_.begin());
        }
        historyIndex_ = history_.size();
    }
    if (line.find_first_not_of(L" \t") == std::wstring::npos) return false;
    pushCommand(std::move(line));
    return true;
}

// ---- command queue -------------------------------------------------------

void ConsoleState::pushCommand(std::wstring line) {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        commandQueue_.push_back(std::move(line));
    }
    queueCv_.notify_one();
}

bool ConsoleState::waitAndPopCommand(std::wstring& out, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(queueMutex_);
    queueCv_.wait_for(lock, timeout, [this] {
        return !commandQueue_.empty() || shutdownRequested.load();
    });
    if (commandQueue_.empty()) return false;
    out = std::move(commandQueue_.front());
    commandQueue_.pop_front();
    return true;
}

// ---- output history ------------------------------------------------------

void ConsoleState::addOutput(std::wstring line, bool isCommand) {
    std::lock_guard<std::mutex> lock(outputMutex_);
    output_.push_back(OutputLine{std::move(line), isCommand});
    if (outputScroll_ > 0) ++outputScroll_;
    while (output_.size() > kMaxOutputLines) {
        output_.pop_front();
        if (outputScroll_ > 0) --outputScroll_;
    }
}

void ConsoleState::addOutputLines(const std::vector<std::wstring>& lines, bool isCommand) {
    std::lock_guard<std::mutex> lock(outputMutex_);
    for (const std::wstring& line : lines) output_.push_back(OutputLine{line, isCommand});
    if (outputScroll_ > 0) outputScroll_ += lines.size();
    while (output_.size() > kMaxOutputLines) {
        output_.pop_front();
        if (outputScroll_ > 0) --outputScroll_;
    }
}

void ConsoleState::clearOutput() {
    std::lock_guard<std::mutex> lock(outputMutex_);
    output_.clear();
    outputScroll_ = 0;
}

std::vector<std::wstring> ConsoleState::outputTail(size_t count) const {
    std::lock_guard<std::mutex> lock(outputMutex_);
    const size_t available = output_.size();
    const size_t take = std::min(count, available);
    std::vector<std::wstring> out;
    out.reserve(take);
    for (auto it = output_.end() - static_cast<ptrdiff_t>(take); it != output_.end(); ++it) {
        out.push_back(it->text);
    }
    return out;
}

std::vector<ConsoleState::OutputLine> ConsoleState::outputWindow(size_t visibleLines) const {
    std::lock_guard<std::mutex> lock(outputMutex_);
    const size_t total = output_.size();
    const size_t hidden = std::min(outputScroll_, total);
    const size_t end = total - hidden;
    const size_t take = std::min(visibleLines, end);
    const size_t start = end - take;
    return std::vector<OutputLine>(output_.begin() + static_cast<ptrdiff_t>(start),
                                    output_.begin() + static_cast<ptrdiff_t>(end));
}

void ConsoleState::setOutputViewportHeight(int height) const {
    outputViewportHeight_.store(std::max(0, height), std::memory_order_relaxed);
}

void ConsoleState::scrollOutputPage(int direction) {
    std::lock_guard<std::mutex> lock(outputMutex_);
    const int viewport = std::max(1, outputViewportHeight_.load(std::memory_order_relaxed));
    const long long page = std::max<long long>(1, viewport - 1);
    const long long total = static_cast<long long>(output_.size());
    const long long maxHidden = std::max<long long>(0, total - viewport);

    long long hidden = static_cast<long long>(outputScroll_) + (direction > 0 ? page : -page);
    hidden = std::clamp<long long>(hidden, 0, maxHidden);
    outputScroll_ = static_cast<size_t>(hidden);
}

void ConsoleState::scrollOutputToBottom() {
    std::lock_guard<std::mutex> lock(outputMutex_);
    outputScroll_ = 0;
}

bool ConsoleState::isOutputScrolled() const {
    std::lock_guard<std::mutex> lock(outputMutex_);
    return outputScroll_ > 0;
}

// ---- input latency -------------------------------------------------------

void ConsoleState::markKeyReceived() {
    long long expected = 0;
    // Only the first keystroke of a burst is stamped; it is the one whose wait
    // for the next frame is longest, so this measures the worst case.
    pendingKeyTicks_.compare_exchange_strong(expected, nowTicks());
}

void ConsoleState::markFramePresented() {
    const long long stamped = pendingKeyTicks_.exchange(0);
    if (stamped != 0) metrics.recordInputLatency(ticksToMs(nowTicks() - stamped));
}

}  // namespace mc
