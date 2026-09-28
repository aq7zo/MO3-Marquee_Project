#include "Diagnostics.h"

#include <windows.h>

#include <cstdio>
#include <string>
#include <vector>

#include "CommandInterpreter.h"
#include "Config.h"
#include "ConsoleState.h"
#include "FrameBuffer.h"
#include "Renderer.h"

namespace mc {

namespace {

std::string toUtf8(const std::wstring& text) {
    if (text.empty()) return {};
    const int needed = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                           nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string out(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(), needed,
                        nullptr, nullptr);
    return out;
}

}  // namespace

int runFrameDump() {
    bool configFound = false;
    const Config config = Config::loadOrDefault("config.txt", &configFound);

    const Renderer renderer(config, config.consoleWidth, config.consoleHeight);
    ConsoleState state(config);
    state.setMarqueeBounds(renderer.layout().marqueeInteriorWidth(),
                           renderer.layout().marqueeInteriorHeight());

    // Drive it into a representative state: marquee running and moved off its
    // origin, plus a transcript that includes a rejected argument.
    CommandInterpreter interpreter(state);
    interpreter.printWelcome(configFound);
    interpreter.execute(L"start_marquee");
    interpreter.execute(L"set_text \"Bouncing marquee text\"");
    interpreter.execute(L"set_speed abc");
    // help is the longest output there is; dumping it proves the whole listing
    // fits the output zone without scrolling.
    interpreter.execute(L"help");
    for (int i = 0; i < 6; ++i) state.tickMarquee();
    state.inputAppend(L's');
    state.inputAppend(L't');
    state.inputAppend(L'a');

    FrameBuffer frame(config.consoleWidth, config.consoleHeight);
    renderer.compose(frame, state);

    const Layout& layout = renderer.layout();
    std::printf("console %dx%d | header %d | marquee rows %d..%d | output rows %d..%d | prompt %d\n",
                layout.width, layout.height, layout.headerHeight, layout.marqueeTop,
                layout.marqueeTop + layout.marqueeHeight - 1, layout.outputTop,
                layout.outputTop + layout.outputHeight - 1, layout.promptRow);

    for (int y = 0; y < frame.height(); ++y) {
        std::wstring row;
        row.reserve(static_cast<size_t>(frame.width()));
        for (int x = 0; x < frame.width(); ++x) {
            row.push_back(frame.data()[static_cast<size_t>(y) * frame.width() + x].Char.UnicodeChar);
        }
        while (!row.empty() && row.back() == L' ') row.pop_back();
        std::printf("%s\n", toUtf8(row).c_str());
    }
    return 0;
}

}  // namespace mc
