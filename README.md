================================================================================
MO3 - MARQUEE CONSOLE
CSOPESY Semi-Major Output 1
================================================================================

NAME
--------------------------------------------------------------------------------
    <<< FILL IN: Your full name >>>
    Group developers are listed in config.ini and shown on the welcome screen.


ENTRY FILE (where main() is located)
--------------------------------------------------------------------------------
    src/main.cpp

    main() loads config.ini, takes over the console, creates the four
    processes, hands them to the Scheduler, and waits for the shutdown signal.


HOW TO RUN
--------------------------------------------------------------------------------
    Option A - Visual Studio 2022 (recommended; this is what the video needs)

        1. Open MarqueeConsole.sln
        2. Set the configuration to Debug | x64
        3. Press F5 (Run/Debug)

        NOTE: this requires the Visual Studio IDE with the "Desktop development
        with C++" workload. Build Tools alone can compile the project but has
        no IDE to press Run/Debug in.

    Option B - Visual Studio Code

        1. Open this folder in VS Code (needs the C/C++ extension)
        2. Press F5

        .vscode/launch.json builds first, then launches in an external
        terminal. The external terminal matters: the program drives a real
        Windows console and will refuse to start inside a pty.

    Option C - Command line

        msbuild MarqueeConsole.sln -p:Configuration=Debug -p:Platform=x64
        build\x64\Debug\MarqueeConsole.exe

        Run it from cmd.exe or Windows Terminal, NOT from Git Bash / MinTTY.


COMMANDS
--------------------------------------------------------------------------------
    Required by the specification:

        help                    displays the commands and its description
        start_marquee           starts the marquee "animation"
        stop_marquee            stops the marquee "animation"
        set_text <text>         accepts a text input and displays it as a marquee
        set_speed <ms>          sets the marquee animation refresh in milliseconds
        exit                    terminates the console

    Additional tuning commands. The quiz only allows changing parameters, not
    recompiling, so the other two rates are reachable at runtime as well:

        set_polling_rate <ms>   keyboard polling interval
        set_refresh <ms>        display refresh interval
        status                  current parameters plus measured FPS and latency
        clear                   clears the output history

    Command names are case-insensitive and surrounding whitespace is ignored.
    set_text keeps the spacing of its argument and accepts optional surrounding
    double quotes.


CONFIGURATION (no rebuild required)
--------------------------------------------------------------------------------
    config.ini holds every startup parameter: console size, marquee text and
    speed, refresh and polling rates, developer names and version date. It is
    read at startup; if it is missing, built-in defaults are used.


DESIGN
--------------------------------------------------------------------------------
    Four processes, each with an independent rate, owned by one Scheduler:

        keyboard      polls the console input buffer      (set_polling_rate)
        marquee       advances the bouncing text          (set_speed)
        display       composes and presents a full frame  (set_refresh)
        interpreter   drains the command queue            (event-driven)

    Each is a Process: its own thread, its own period, and a uniform
    start/stop/join lifecycle. Waiting is done with a condition_variable timed
    wait against an absolute deadline, so there is no busy-waiting and no drift,
    and a process shuts down immediately regardless of how long its period is.

    Rendering is tear-free: a whole frame is composed into an off-screen
    CHAR_INFO buffer and pushed to the console with a single WriteConsoleOutputW
    call, so a partially drawn frame can never be visible. The real cursor is
    hidden and the caret is drawn into the frame instead.

    Separating the three rates is what makes the refresh-versus-polling
    trade-off measurable. Use `status` to read back actual FPS, frame compose
    time, poll interval and input-to-display latency.


DIAGNOSTIC FLAGS
--------------------------------------------------------------------------------
    MarqueeConsole.exe --selftest    unit tests for the marquee geometry,
                                     command parsing and config clamping;
                                     needs no console, exits non-zero on failure

    MarqueeConsole.exe --dumpframe   composes one frame and prints it as text,
                                     for checking the layout without a console


FILE MAP
--------------------------------------------------------------------------------
    src/main.cpp                 entry point, wiring, shutdown
    src/Config.*                 config.ini parsing, defaults, bounds
    src/ConsoleScreen.*          Win32 console RAII, raw mode, frame presentation
    src/FrameBuffer.*            CHAR_INFO grid and drawing primitives
    src/Renderer.*               layout, CSOPESY banner, frame composition
    src/ConsoleState.*           shared state and its locking
    src/MarqueeState.*           bounce geometry (pure logic, unit-tested)
    src/CommandParser.*          tokenising and validation (pure logic, tested)
    src/CommandInterpreter.*     command dispatch and handlers
    src/Process.*                periodic worker base class
    src/Scheduler.*              process lifecycle ownership
    src/MarqueeProcess.*         animation tick
    src/DisplayProcess.*         frame composition and presentation
    src/KeyboardProcess.*        input polling
    src/InterpreterProcess.*     command queue drain
    src/Metrics.*                FPS and latency instrumentation
    src/SelfTest.*               unit tests
    src/Diagnostics.*            headless frame dump
    specs/SPEC.md                distilled specification and rubric checklist
