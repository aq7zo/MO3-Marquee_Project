#include "SelfTest.h"

#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>

#include "CommandInterpreter.h"
#include "CommandParser.h"
#include "Config.h"
#include "ConsoleState.h"
#include "InterpreterProcess.h"
#include "MarqueeProcess.h"
#include "MarqueeState.h"
#include "Scheduler.h"

namespace mc {

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool condition, const char* what) {
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::printf("  FAIL: %s\n", what);
    }
}

void checkEq(int actual, int expected, const char* what) {
    ++g_checks;
    if (actual != expected) {
        ++g_failures;
        std::printf("  FAIL: %s (expected %d, got %d)\n", what, expected, actual);
    }
}

// ---- MarqueeState --------------------------------------------------------

void testMarqueeBouncesOffRightEdge() {
    MarqueeState m;
    m.setBounds(10, 3);
    m.setText(L"abc");  // width 3, so the last valid x is 7

    for (int i = 0; i < 7; ++i) m.tick();
    checkEq(m.x(), 7, "marquee reaches the right edge");
    checkEq(m.dx(), 1, "direction still rightward at the edge");

    m.tick();
    checkEq(m.x(), 7, "marquee holds the edge for one tick");
    checkEq(m.dx(), -1, "direction reverses at the right edge");

    m.tick();
    checkEq(m.x(), 6, "marquee moves back left after bouncing");
}

void testMarqueeBouncesOffLeftEdge() {
    MarqueeState m;
    m.setBounds(10, 3);
    m.setText(L"abc");

    m.tick();  // x = 1
    for (int i = 0; i < 20; ++i) m.tick();
    check(m.x() >= 0 && m.x() <= 7, "x stays within bounds across many ticks");

    MarqueeState left;
    left.setBounds(10, 3);
    left.setText(L"abc");
    // Drive it to the right edge, back to the left, and past it.
    for (int i = 0; i < 16; ++i) left.tick();
    checkEq(left.x(), 0, "marquee returns to the left edge");
    left.tick();
    checkEq(left.dx(), 1, "direction reverses at the left edge");
}

void testMarqueeBouncesVertically() {
    MarqueeState m;
    m.setBounds(20, 3);  // rows 0..2
    m.setText(L"hi");

    checkEq(m.y(), 0, "starts on the top row");
    m.tick();
    checkEq(m.y(), 1, "moves down");
    m.tick();
    checkEq(m.y(), 2, "reaches the bottom row");
    m.tick();
    checkEq(m.y(), 2, "holds the bottom row for one tick");
    checkEq(m.dy(), -1, "direction reverses at the bottom");
    m.tick();
    checkEq(m.y(), 1, "moves back up");
}

void testMarqueeCornerReversesBothAxes() {
    MarqueeState m;
    m.setBounds(5, 3);
    m.setText(L"ab");  // maxX = 3, maxY = 2

    m.tick();  // (1,1)
    m.tick();  // (2,2)
    m.tick();  // x=3 (max), y holds at 2 and dy flips
    checkEq(m.x(), 3, "x at maximum in the corner");
    checkEq(m.y(), 2, "y at maximum in the corner");
    m.tick();
    checkEq(m.dx(), -1, "x direction reversed after the corner");
    checkEq(m.dy(), -1, "y direction reversed after the corner");
}

void testMarqueeHandlesOversizedText() {
    MarqueeState m;
    m.setBounds(5, 3);
    m.setText(L"this text is far wider than the zone");
    check(m.textOverflows(), "oversized text is reported as overflowing");
    for (int i = 0; i < 10; ++i) m.tick();
    checkEq(m.x(), 0, "oversized text stays pinned to the left edge");
    check(m.y() >= 0 && m.y() < 3, "oversized text still moves vertically in bounds");
}

void testMarqueeHandlesDegenerateInput() {
    MarqueeState empty;
    empty.setBounds(10, 3);
    empty.setText(L"");
    for (int i = 0; i < 5; ++i) empty.tick();
    checkEq(empty.x(), 0, "empty text stays at origin");

    MarqueeState zero;
    zero.setBounds(0, 0);
    zero.setText(L"abc");
    for (int i = 0; i < 5; ++i) zero.tick();  // must not crash or go out of range
    checkEq(zero.x(), 0, "zero-width zone keeps x at 0");
    checkEq(zero.y(), 0, "zero-height zone keeps y at 0");
}

void testMarqueeReclampsWhenTextGrows() {
    MarqueeState m;
    m.setBounds(10, 3);
    m.setText(L"ab");
    for (int i = 0; i < 8; ++i) m.tick();  // pushes x toward the right edge
    m.setText(L"abcdefgh");                // now 8 wide, so maxX drops to 2
    check(m.x() <= 2, "position is re-clamped when the text grows");
}

// ---- CommandParser -------------------------------------------------------

void testParserResolvesEveryCommand() {
    for (const CommandInfo& info : commandCatalog()) {
        const ParsedCommand cmd = parser::parse(info.name);
        check(cmd.id == info.id, "catalog command resolves to its own id");
    }
}

void testParserIsCaseInsensitiveAndTrims() {
    check(parser::parse(L"HELP").id == CommandId::Help, "uppercase HELP resolves");
    check(parser::parse(L"Help").id == CommandId::Help, "mixed-case Help resolves");
    check(parser::parse(L"   help   ").id == CommandId::Help, "surrounding whitespace ignored");
    check(parser::parse(L"START_MARQUEE").id == CommandId::StartMarquee, "uppercase start_marquee");
}

void testParserHandlesEmptyAndUnknown() {
    check(parser::parse(L"").id == CommandId::Empty, "empty line is Empty");
    check(parser::parse(L"    ").id == CommandId::Empty, "whitespace-only line is Empty");
    check(parser::parse(L"\t \r\n").id == CommandId::Empty, "mixed whitespace is Empty");

    const ParsedCommand unknown = parser::parse(L"frobnicate 12");
    check(unknown.id == CommandId::Unknown, "unrecognised verb is Unknown");
    check(unknown.name == L"frobnicate", "unknown command keeps the name as typed");
}

void testParserPreservesSetTextSpacing() {
    const ParsedCommand cmd = parser::parse(L"set_text   Hello   brave  world  ");
    check(cmd.id == CommandId::SetText, "set_text resolves");
    check(cmd.argRaw == L"Hello   brave  world",
          "internal spacing is preserved and the tail is trimmed");
    checkEq(static_cast<int>(cmd.args.size()), 3, "args still split for commands that want them");
}

void testParserStripsQuotes() {
    check(parser::stripSurroundingQuotes(L"\"Hello world\"") == L"Hello world",
          "matching quotes are stripped");
    check(parser::stripSurroundingQuotes(L"Hello world") == L"Hello world",
          "unquoted text is unchanged");
    check(parser::stripSurroundingQuotes(L"\"unbalanced") == L"\"unbalanced",
          "a lone leading quote is left alone");
    check(parser::stripSurroundingQuotes(L"\"\"") == L"", "empty quoted string becomes empty");
    check(parser::stripSurroundingQuotes(L"say \"hi\" now") == L"say \"hi\" now",
          "interior quotes are untouched");
}

void testParseIntAcceptsValidNumbers() {
    int value = -1;
    check(parser::parseInt(L"100", value) == NumberParse::Ok && value == 100, "plain integer");
    check(parser::parseInt(L"+42", value) == NumberParse::Ok && value == 42, "leading plus");
    check(parser::parseInt(L"  7  ", value) == NumberParse::Ok && value == 7, "padded integer");
    check(parser::parseInt(L"0", value) == NumberParse::Ok && value == 0, "zero parses");
    check(parser::parseInt(L"-0", value) == NumberParse::Ok && value == 0, "negative zero is zero");
}

void testParseIntRejectsBadInput() {
    int value = 0;
    check(parser::parseInt(L"", value) == NumberParse::Empty, "empty is rejected");
    check(parser::parseInt(L"abc", value) == NumberParse::NotANumber, "letters are rejected");
    check(parser::parseInt(L"12abc", value) == NumberParse::NotANumber, "trailing junk is rejected");
    check(parser::parseInt(L"1.5", value) == NumberParse::NotANumber, "decimals are rejected");
    check(parser::parseInt(L"-5", value) == NumberParse::Negative, "negatives are flagged");
    check(parser::parseInt(L"99999999999999", value) == NumberParse::OutOfRange,
          "overflow is caught rather than wrapping");
    check(parser::parseInt(L"+", value) == NumberParse::NotANumber, "a lone sign is rejected");
}

// ---- Config --------------------------------------------------------------

void testConfigClamping() {
    Config cfg;
    cfg.marqueeSpeedMs = -50;
    cfg.refreshMs = 999999;
    cfg.pollingMs = 0;
    cfg.consoleWidth = 5;
    cfg.clampAll();

    check(cfg.marqueeSpeedMs >= limits::kMinSpeedMs, "negative speed is clamped up");
    check(cfg.refreshMs <= limits::kMaxRefreshMs, "huge refresh is clamped down");
    check(cfg.pollingMs >= limits::kMinPollingMs, "zero polling is clamped up");
    check(cfg.consoleWidth >= 60, "tiny console width is clamped up");
    check(cfg.marqueeZoneHeight >= 3, "marquee zone keeps a usable height");
}

void testMissingConfigFallsBackToDefaults() {
    bool found = true;
    const Config cfg = Config::loadOrDefault("definitely-not-a-real-file.ini", &found);
    check(!found, "a missing config file is reported as not found");
    check(cfg.marqueeSpeedMs == Config{}.marqueeSpeedMs, "defaults survive a missing file");
}

// ---- Scheduler and processes --------------------------------------------

// Exercises the live threading without a console: the display and keyboard
// processes are left out (they need Win32 handles), but the marquee and
// interpreter cover process startup, command execution, immediate rate changes
// and clean shutdown - the parts most likely to hang or race.
void testSchedulerRunsCommandsAndStops() {
    using namespace std::chrono;

    Config cfg;
    cfg.marqueeSpeedMs = 10;
    ConsoleState state(cfg);
    state.setMarqueeBounds(40, 5);

    CommandInterpreter interpreter(state);
    Scheduler scheduler;
    Process* marquee = scheduler.add(std::make_unique<MarqueeProcess>(state));
    scheduler.add(std::make_unique<InterpreterProcess>(state, interpreter));

    CommandInterpreter::ProcessHandles handles;
    handles.marquee = marquee;
    interpreter.attach(&scheduler, handles);

    scheduler.startAll();

    check(state.marqueeSnapshot().x == 0, "marquee starts at the origin");

    state.pushCommand(L"start_marquee");
    std::this_thread::sleep_for(milliseconds(250));
    check(state.marqueeRunning.load(), "start_marquee was executed by the interpreter process");
    check(state.marqueeSnapshot().x > 0, "marquee process actually advanced the position");

    state.pushCommand(L"set_speed 25");
    std::this_thread::sleep_for(milliseconds(150));
    checkEq(state.marqueeSpeedMs.load(), 25, "set_speed updated the shared parameter");
    checkEq(marquee->periodMs(), 25, "set_speed retuned the marquee process immediately");

    state.pushCommand(L"stop_marquee");
    std::this_thread::sleep_for(milliseconds(150));
    check(!state.marqueeRunning.load(), "stop_marquee halted the animation");
    const int frozen = state.marqueeSnapshot().x;
    std::this_thread::sleep_for(milliseconds(150));
    checkEq(state.marqueeSnapshot().x, frozen, "position stays frozen while stopped");

    state.pushCommand(L"exit");
    std::this_thread::sleep_for(milliseconds(150));
    check(state.shutdownRequested.load(), "exit requested shutdown");

    // The real risk here is a shutdown that hangs, so time it rather than just
    // waiting for it.
    const auto begin = steady_clock::now();
    scheduler.stopAll();
    const auto elapsed = duration_cast<milliseconds>(steady_clock::now() - begin).count();
    check(elapsed < 1000, "every process joined promptly on shutdown");
}

// A process with a very long period must still stop instantly, because the wait
// predicate watches the stop flag rather than just the deadline.
void testLongPeriodProcessStopsImmediately() {
    using namespace std::chrono;

    Config cfg;
    ConsoleState state(cfg);
    Scheduler scheduler;
    Process* marquee = scheduler.add(std::make_unique<MarqueeProcess>(state));
    marquee->setPeriodMs(limits::kMaxSpeedMs);  // 60 seconds
    scheduler.startAll();
    std::this_thread::sleep_for(milliseconds(50));

    const auto begin = steady_clock::now();
    scheduler.stopAll();
    const auto elapsed = duration_cast<milliseconds>(steady_clock::now() - begin).count();
    check(elapsed < 500, "a 60-second period does not delay shutdown");
}

// ---- ConsoleState input line ---------------------------------------------

void typeLine(ConsoleState& state, const std::wstring& text) {
    for (wchar_t c : text) state.inputAppend(c);
}

void testInputCursorEditing() {
    using CursorMove = ConsoleState::CursorMove;
    Config cfg;
    ConsoleState state(cfg);

    typeLine(state, L"hello world");
    checkEq(state.inputSnapshot().cursor, 11, "caret sits at the end after typing");

    state.inputMoveCursor(CursorMove::WordLeft);
    checkEq(state.inputSnapshot().cursor, 6, "Ctrl+Left lands on the last word");
    state.inputMoveCursor(CursorMove::WordLeft);
    checkEq(state.inputSnapshot().cursor, 0, "Ctrl+Left again lands on the first word");
    state.inputMoveCursor(CursorMove::WordLeft);
    checkEq(state.inputSnapshot().cursor, 0, "Ctrl+Left stops at the start of the line");

    state.inputMoveCursor(CursorMove::WordRight);
    checkEq(state.inputSnapshot().cursor, 6, "Ctrl+Right is the inverse of Ctrl+Left");

    state.inputMoveCursor(CursorMove::Left);
    state.inputAppend(L',');
    check(state.inputSnapshot().text == L"hello, world", "typing inserts at the caret");

    state.inputBackspace();
    check(state.inputSnapshot().text == L"hello world", "backspace deletes before the caret");

    state.inputMoveCursor(CursorMove::End);
    checkEq(state.inputSnapshot().cursor, 11, "End goes to the end of the line");
    state.inputMoveCursor(CursorMove::Right);
    checkEq(state.inputSnapshot().cursor, 11, "Right stops at the end of the line");
    state.inputMoveCursor(CursorMove::Home);
    checkEq(state.inputSnapshot().cursor, 0, "Home goes to the start of the line");
}

void testInputKills() {
    using CursorMove = ConsoleState::CursorMove;
    Config cfg;
    ConsoleState state(cfg);

    typeLine(state, L"set_text hello");
    state.inputMoveCursor(CursorMove::WordLeft);
    state.inputKillToEnd();
    check(state.inputSnapshot().text == L"set_text ", "Ctrl+K cuts to the end of the line");
    checkEq(state.inputSnapshot().cursor, 9, "Ctrl+K leaves the caret where it was");

    state.inputKillToStart();
    check(state.inputSnapshot().text.empty(), "Ctrl+U cuts back to the start of the line");
    checkEq(state.inputSnapshot().cursor, 0, "Ctrl+U puts the caret at column zero");
}

void testInputHistory() {
    using HistoryStep = ConsoleState::HistoryStep;
    Config cfg;
    ConsoleState state(cfg);

    typeLine(state, L"help");
    state.submitInputLine();
    typeLine(state, L"status");
    state.submitInputLine();
    typeLine(state, L"status");
    state.submitInputLine();  // a repeat: must not take its own history slot
    typeLine(state, L"draft");

    state.inputRecallHistory(HistoryStep::Prev);
    check(state.inputSnapshot().text == L"status", "Up recalls the last command");
    checkEq(state.inputSnapshot().cursor, 6, "a recalled line puts the caret at its end");

    state.inputRecallHistory(HistoryStep::Prev);
    check(state.inputSnapshot().text == L"help", "Up again skips the duplicate entry");
    state.inputRecallHistory(HistoryStep::Prev);
    check(state.inputSnapshot().text == L"help", "Up stops at the oldest entry");

    state.inputRecallHistory(HistoryStep::Next);
    check(state.inputSnapshot().text == L"status", "Down walks back towards the newest");
    state.inputRecallHistory(HistoryStep::Next);
    check(state.inputSnapshot().text == L"draft", "Down past the newest restores the draft");
    state.inputRecallHistory(HistoryStep::Next);
    check(state.inputSnapshot().text == L"draft", "Down on the draft line is a no-op");
}

void testBlankLineIsNotRemembered() {
    using HistoryStep = ConsoleState::HistoryStep;
    Config cfg;
    ConsoleState state(cfg);

    typeLine(state, L"   ");
    check(!state.submitInputLine(), "a blank line is not submitted");
    state.inputRecallHistory(HistoryStep::Prev);
    check(state.inputSnapshot().text.empty(), "a blank line is not added to the history");
}

}  // namespace

int runSelfTest() {
    std::printf("MarqueeConsole self-test\n");

    std::printf(" MarqueeState\n");
    testMarqueeBouncesOffRightEdge();
    testMarqueeBouncesOffLeftEdge();
    testMarqueeBouncesVertically();
    testMarqueeCornerReversesBothAxes();
    testMarqueeHandlesOversizedText();
    testMarqueeHandlesDegenerateInput();
    testMarqueeReclampsWhenTextGrows();

    std::printf(" CommandParser\n");
    testParserResolvesEveryCommand();
    testParserIsCaseInsensitiveAndTrims();
    testParserHandlesEmptyAndUnknown();
    testParserPreservesSetTextSpacing();
    testParserStripsQuotes();
    testParseIntAcceptsValidNumbers();
    testParseIntRejectsBadInput();

    std::printf(" Config\n");
    testConfigClamping();
    testMissingConfigFallsBackToDefaults();

    std::printf(" ConsoleState input line\n");
    testInputCursorEditing();
    testInputKills();
    testInputHistory();
    testBlankLineIsNotRemembered();

    std::printf(" Scheduler / processes (live threads)\n");
    testSchedulerRunsCommandsAndStops();
    testLongPeriodProcessStopsImmediately();

    std::printf("%d checks, %d failure(s)\n", g_checks, g_failures);
    return (g_failures == 0) ? 0 : 1;
}

}  // namespace mc
