#include "Renderer.h"

#include <algorithm>
#include <chrono>

namespace mc {

namespace {

constexpr wchar_t kBlock = L'\u2588';
constexpr int kGlyphRows = 5;
constexpr int kGlyphWidth = 7;

// Block letters spelling CSOPESY. Held as plain ASCII with '#' standing in for
// the solid block, so this file has no non-ASCII characters to be mangled by a
// source-encoding change; the substitution happens when the banner is built.
const char* const kGlyphs[][kGlyphRows] = {
    {" ##### ", "##     ", "##     ", "##     ", " ##### "},  // C
    {" ##### ", "##     ", " ##### ", "     ##", " ##### "},  // S
    {" ##### ", "##   ##", "##   ##", "##   ##", " ##### "},  // O
    {"###### ", "##   ##", "###### ", "##     ", "##     "},  // P
    {"#######", "##     ", "#####  ", "##     ", "#######"},  // E
    {" ##### ", "##     ", " ##### ", "     ##", " ##### "},  // S
    {"##   ##", " ## ## ", "  ###  ", "   ##  ", "   ##  "},  // Y
};
constexpr int kGlyphCount = static_cast<int>(std::size(kGlyphs));

std::vector<std::wstring> buildBanner() {
    std::vector<std::wstring> rows(kGlyphRows);
    for (int row = 0; row < kGlyphRows; ++row) {
        std::wstring line;
        line.reserve(static_cast<size_t>(kGlyphCount) * (kGlyphWidth + 1));
        for (int glyph = 0; glyph < kGlyphCount; ++glyph) {
            if (glyph > 0) line.push_back(L' ');
            for (const char* p = kGlyphs[glyph][row]; *p != '\0'; ++p) {
                line.push_back(*p == '#' ? kBlock : L' ');
            }
        }
        rows[row] = std::move(line);
    }
    return rows;
}

bool caretVisible() {
    using namespace std::chrono;
    const auto ms = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    return (ms / 500) % 2 == 0;
}

}  // namespace

Renderer::Renderer(const Config& config, int consoleWidth, int consoleHeight)
    : config_(config), banner_(buildBanner()) {
    layout_.width = consoleWidth;
    layout_.height = consoleHeight;

    // Header: greeting, blank, banner, blank, developer list, blank,
    // version date, blank.
    const int developerCount = static_cast<int>(config_.developers.size());
    layout_.headerHeight = 1 + 1 + kGlyphRows + 1 + 1 + developerCount + 1 + 1 + 1;

    layout_.hintRow = consoleHeight - 1;
    layout_.promptRow = consoleHeight - 2;
    layout_.marqueeTop = layout_.headerHeight;
    layout_.marqueeHeight = config_.marqueeZoneHeight;

    // Give the output zone at least a usable interior; if the console is too
    // short, take the space back from the marquee rather than letting the zones
    // collide.
    constexpr int kMinOutputHeight = 5;
    const int availableBelowHeader = layout_.promptRow - 1 - layout_.marqueeTop;
    if (layout_.marqueeHeight + kMinOutputHeight > availableBelowHeader) {
        layout_.marqueeHeight = std::max(3, availableBelowHeader - kMinOutputHeight);
    }

    layout_.outputTop = layout_.marqueeTop + layout_.marqueeHeight;
    layout_.outputHeight = std::max(0, layout_.promptRow - 1 - layout_.outputTop);
}

void Renderer::compose(FrameBuffer& fb, const ConsoleState& state) const {
    fb.clear(color::kGray);
    drawHeader(fb);
    drawMarquee(fb, state);
    drawOutput(fb, state);
    drawPrompt(fb, state);
}

void Renderer::drawHeader(FrameBuffer& fb) const {
    int row = 0;
    fb.drawText(2, row++, L"Welcome to CSOPESY!", color::kWhite);
    ++row;

    for (const std::wstring& line : banner_) {
        fb.drawText(2, row++, line, color::kCyan);
    }
    ++row;

    fb.drawText(2, row++, L"Group developers:", color::kYellow);
    for (const std::wstring& developer : config_.developers) {
        fb.drawText(2, row++, developer, color::kGray);
    }
    ++row;

    // Build version, right-aligned on the version-date row. It shares the row
    // rather than taking its own so headerHeight - and every zone below it -
    // stays exactly where it was; the "Version date:" line itself is graded
    // against the spec mockup and is left verbatim.
    const std::wstring build = L"v" + config_.version;
    fb.drawText(layout_.width - static_cast<int>(build.size()) - 2, row, build, color::kDarkGray);
    fb.drawText(2, row++, L"Version date: " + config_.versionDate, color::kYellow);
}

void Renderer::drawMarquee(FrameBuffer& fb, const ConsoleState& state) const {
    const bool running = state.marqueeRunning.load();
    const WORD borderAttr = running ? color::kGreen : color::kDarkGray;

    fb.drawBox(0, layout_.marqueeTop, layout_.width, layout_.marqueeHeight, borderAttr);

    const std::wstring title = running ? L"[ marquee: running ]" : L"[ marquee: stopped ]";
    fb.drawText(3, layout_.marqueeTop, title, borderAttr);

    const ConsoleState::MarqueeSnapshot snap = state.marqueeSnapshot();
    if (snap.text.empty()) return;

    fb.drawTextClipped(1 + snap.x,
                       layout_.marqueeTop + 1 + snap.y,
                       layout_.marqueeInteriorWidth(),
                       snap.text,
                       color::kMagenta);
}

void Renderer::drawOutput(FrameBuffer& fb, const ConsoleState& state) const {
    if (layout_.outputHeight < 3) return;

    fb.drawBox(0, layout_.outputTop, layout_.width, layout_.outputHeight, color::kDarkGray);
    fb.drawText(3, layout_.outputTop, L"[ output ]", color::kDarkGray);

    const int interior = layout_.outputInteriorHeight();
    const std::vector<std::wstring> lines = state.outputTail(static_cast<size_t>(interior));
    for (size_t i = 0; i < lines.size(); ++i) {
        fb.drawTextClipped(2,
                           layout_.outputTop + 1 + static_cast<int>(i),
                           layout_.width - 3,
                           lines[i],
                           color::kGray);
    }
}

void Renderer::drawPrompt(FrameBuffer& fb, const ConsoleState& state) const {
    const std::wstring prompt = L"Command> ";
    fb.drawText(1, layout_.promptRow, prompt, color::kWhite);

    const ConsoleState::InputSnapshot input = state.inputSnapshot();
    const int inputLeft = 1 + static_cast<int>(prompt.size());
    const int room = layout_.width - inputLeft - 2;

    // Scroll the visible window so the caret stays on screen. The caret can sit
    // anywhere in the line now, so following the tail is not enough: editing the
    // start of a long line has to scroll back to it.
    int scroll = 0;
    if (room > 0 && input.cursor > room - 1) scroll = input.cursor - (room - 1);

    std::wstring visible = input.text.substr(static_cast<size_t>(scroll));
    if (room > 0 && static_cast<int>(visible.size()) > room) {
        visible.resize(static_cast<size_t>(room));
    }
    fb.drawText(inputLeft, layout_.promptRow, visible, color::kGreen);

    if (caretVisible()) {
        fb.putChar(inputLeft + input.cursor - scroll,
                   layout_.promptRow,
                   L'\u2588',
                   color::kGreen);
    }

    fb.drawText(1, layout_.hintRow, L"Type 'help' for the command list.", color::kDarkGray);
}

}  // namespace mc
