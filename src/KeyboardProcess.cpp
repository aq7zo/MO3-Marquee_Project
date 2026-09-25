#include "KeyboardProcess.h"

#include <array>
#include <cstring>
#include <string>

namespace mc {

namespace {

void copyToClipboard(const std::wstring& text) {
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    if (HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes)) {
        if (void* dst = GlobalLock(mem)) {
            std::memcpy(dst, text.c_str(), bytes);
            GlobalUnlock(mem);
            // On success the clipboard owns the memory; on failure it is ours.
            if (!SetClipboardData(CF_UNICODETEXT, mem)) GlobalFree(mem);
        } else {
            GlobalFree(mem);
        }
    }
    CloseClipboard();
}

std::wstring readClipboard() {
    std::wstring text;
    if (!OpenClipboard(nullptr)) return text;
    if (HANDLE mem = GetClipboardData(CF_UNICODETEXT)) {
        if (const auto* src = static_cast<const wchar_t*>(GlobalLock(mem))) {
            text = src;
            GlobalUnlock(mem);
        }
    }
    CloseClipboard();
    return text;
}

}  // namespace

KeyboardProcess::KeyboardProcess(ConsoleState& state, HANDLE input)
    : Process("keyboard", state.pollingMs.load()), state_(state), input_(input) {}

void KeyboardProcess::tick() {
    const auto now = std::chrono::steady_clock::now();
    if (havePolled_) {
        state_.metrics.recordPoll(std::chrono::duration<double, std::milli>(now - lastPoll_).count());
    }
    lastPoll_ = now;
    havePolled_ = true;

    if (input_ == INVALID_HANDLE_VALUE) return;

    // Drain everything queued since the last poll. Reading only one event per
    // tick would make fast typing fall progressively further behind.
    DWORD pending = 0;
    if (!GetNumberOfConsoleInputEvents(input_, &pending) || pending == 0) return;

    std::array<INPUT_RECORD, 64> records{};
    while (pending > 0) {
        DWORD read = 0;
        const DWORD want = (pending < records.size()) ? pending : static_cast<DWORD>(records.size());
        if (!ReadConsoleInputW(input_, records.data(), want, &read) || read == 0) return;

        for (DWORD i = 0; i < read; ++i) {
            if (records[i].EventType != KEY_EVENT) continue;
            if (!records[i].Event.KeyEvent.bKeyDown) continue;
            handleKeyEvent(records[i].Event.KeyEvent);
        }

        pending = (pending > read) ? pending - read : 0;
    }
}

void KeyboardProcess::handleKeyEvent(const KEY_EVENT_RECORD& key) {
    using CursorMove = ConsoleState::CursorMove;
    using HistoryStep = ConsoleState::HistoryStep;

    // Every accepted keystroke is stamped before it is applied, and only
    // accepted ones: a bare Ctrl or an unbound function key must not start a
    // latency measurement that no redraw will ever close.
    const auto edit = [this](auto&& apply) {
        state_.markKeyReceived();
        apply();
    };

    const wchar_t ch = key.uChar.UnicodeChar;
    const bool ctrlHeld =
        (key.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;

    // PROCESSED_INPUT is disabled, so Ctrl+C arrives here as an ordinary key.
    // There is no selection, so it copies the whole input line; `exit` quits.
    if (ctrlHeld && key.wVirtualKeyCode == 'C') {
        state_.markKeyReceived();
        copyToClipboard(state_.inputSnapshot().text);
        return;
    }

    // Pasted text goes through inputAppend one character at a time, so it
    // inserts at the caret and respects the input length limit. Line breaks
    // and other control characters are dropped to keep it a single line.
    if (ctrlHeld && key.wVirtualKeyCode == 'V') {
        state_.markKeyReceived();
        for (wchar_t c : readClipboard()) {
            if (c >= L' ' && c != 0x7F) state_.inputAppend(c);
        }
        return;
    }

    // Ctrl-chorded editing keys. These arrive with a control character in
    // uChar (Ctrl+A is 0x01 and so on), which the printable filter at the
    // bottom would drop silently, so they are dispatched here first.
    if (ctrlHeld) {
        switch (key.wVirtualKeyCode) {
            case VK_LEFT:
                return edit([this] { state_.inputMoveCursor(CursorMove::WordLeft); });
            case VK_RIGHT:
                return edit([this] { state_.inputMoveCursor(CursorMove::WordRight); });
            case 'A':
                return edit([this] { state_.inputMoveCursor(CursorMove::Home); });
            case 'E':
                return edit([this] { state_.inputMoveCursor(CursorMove::End); });
            case 'P':
                return edit([this] { state_.inputRecallHistory(HistoryStep::Prev); });
            case 'N':
                return edit([this] { state_.inputRecallHistory(HistoryStep::Next); });
            case 'U':
                return edit([this] { state_.inputKillToStart(); });
            case 'K':
                return edit([this] { state_.inputKillToEnd(); });
            default:
                break;
        }
    }

    switch (key.wVirtualKeyCode) {
        case VK_LEFT:
            return edit([this] { state_.inputMoveCursor(CursorMove::Left); });
        case VK_RIGHT:
            return edit([this] { state_.inputMoveCursor(CursorMove::Right); });
        case VK_HOME:
            return edit([this] { state_.inputMoveCursor(CursorMove::Home); });
        case VK_END:
            return edit([this] { state_.inputMoveCursor(CursorMove::End); });
        case VK_PRIOR: 
            return edit([this] { state_.scrollOutputPage(+1); });
        case VK_NEXT:   
            return edit([this] { state_.scrollOutputPage(-1); });
        case VK_UP:
            return edit([this] { state_.inputRecallHistory(HistoryStep::Prev); });
        case VK_DOWN:
            return edit([this] { state_.inputRecallHistory(HistoryStep::Next); });
        case VK_RETURN:
            state_.markKeyReceived();
            state_.submitInputLine();
            return;
        case VK_BACK:
            state_.markKeyReceived();
            state_.inputBackspace();
            return;
        case VK_DELETE:
            return edit([this] { state_.inputDelete(); });
        case VK_ESCAPE:
            state_.markKeyReceived();
            state_.inputClear();
            return;
        default:
            break;
    }

    // Printable characters only: this filters out modifier-only presses, arrow
    // keys and function keys, all of which report a zero or control character.
    if (ch >= L' ' && ch != 0x7F) {
        state_.markKeyReceived();
        state_.inputAppend(ch);
    }
}

}  // namespace mc
