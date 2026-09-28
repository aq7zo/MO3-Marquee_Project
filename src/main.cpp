// MO3 - Marquee Console (CSOPESY Semi-Major Output 1)
//
// ENTRY POINT: this file contains main(). See README.txt.
//
// Four concurrent processes, each with its own independent rate, coordinated by
// one Scheduler:
//
//   keyboard    polls the console input buffer          (set_polling_rate)
//   marquee     advances the bouncing text              (set_speed)
//   display     composes and presents a whole frame     (set_refresh)
//   interpreter drains and executes the command queue   (event-driven)
//
// Keeping those rates separate is what lets the marquee animate smoothly while
// typing stays responsive, and is what makes the refresh-versus-polling
// trade-off measurable via the `status` command.

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <thread>

#include "CommandInterpreter.h"
#include "Config.h"
#include "Diagnostics.h"
#include "ConsoleScreen.h"
#include "ConsoleState.h"
#include "DisplayProcess.h"
#include "InterpreterProcess.h"
#include "KeyboardProcess.h"
#include "MarqueeProcess.h"
#include "Renderer.h"
#include "Scheduler.h"
#include "SelfTest.h"

namespace {

constexpr const char* kConfigPath = "config.txt";

bool hasFlag(int argc, char** argv, const char* flag) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], flag) == 0) return true;
    }
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    if (hasFlag(argc, argv, "--selftest")) {
        return mc::runSelfTest();
    }
    if (hasFlag(argc, argv, "--dumpframe")) {
        return mc::runFrameDump();
    }

    bool configFound = false;
    const mc::Config config = mc::Config::loadOrDefault(kConfigPath, &configFound);

    mc::ConsoleScreen screen(config.consoleWidth, config.consoleHeight,
                             L"MO3 - Marquee Console (CSOPESY)");
    if (!screen.attached()) {
        std::printf(
            "MarqueeConsole needs a real Windows console.\n"
            "Run it from Visual Studio (Run/Debug), cmd.exe or Windows Terminal -\n"
            "not from a pty such as MinTTY/Git Bash.\n"
            "Use --selftest to run the logic tests without a console.\n");
        return 1;
    }

    const mc::Renderer renderer(config, screen.width(), screen.height());

    mc::ConsoleState state(config);
    state.setMarqueeBounds(renderer.layout().marqueeInteriorWidth(),
                           renderer.layout().marqueeInteriorHeight());

    // Declared before the Scheduler so it is destroyed *after* it: Scheduler's
    // destruction joins the interpreter thread, which must not outlive the
    // interpreter it calls into.
    mc::CommandInterpreter interpreter(state);

    mc::Scheduler scheduler;
    mc::Process* marquee = scheduler.add(std::make_unique<mc::MarqueeProcess>(state));
    mc::Process* display =
        scheduler.add(std::make_unique<mc::DisplayProcess>(state, screen, renderer));
    mc::Process* keyboard =
        scheduler.add(std::make_unique<mc::KeyboardProcess>(state, screen.inputHandle()));
    scheduler.add(std::make_unique<mc::InterpreterProcess>(state, interpreter));

    mc::CommandInterpreter::ProcessHandles handles;
    handles.marquee = marquee;
    handles.display = display;
    handles.keyboard = keyboard;
    interpreter.attach(&scheduler, handles);

    interpreter.printWelcome(configFound);
    scheduler.startAll();

    // The main thread only waits for the shutdown signal; all work happens in
    // the processes. This sleep is the idle path, so it is a real sleep rather
    // than a spin.
    while (!state.shutdownRequested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    scheduler.stopAll();
    return 0;
}
