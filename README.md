# MO3 — Marquee Console

A Windows console marquee built for CSOPESY Semi-Major Output 1: four concurrent
processes, each running at its own independent rate, coordinated by a single
scheduler and drawn tear-free into one off-screen frame buffer.

```
+--------------------------------------------------------+
|                                                        |
|                      Hello world!                      |
|                                                        |
+--------------------------------------------------------+
Enter a command> set_speed 30
```

Keeping the animation, display and input rates separate is the point of the
exercise: the refresh-versus-polling trade-off becomes something you can measure
at runtime with `status`, not something you argue about.

## Requirements

- Windows (the project uses the Win32 console API directly)
- `g++` with C++17 support — MinGW-w64 or MSYS2
- GNU Make

> [!IMPORTANT]
> Run the program from `cmd.exe` or Windows Terminal, **not** from Git Bash /
> MinTTY. It drives a real Windows console and will refuse to start inside a pty.

## Quick start

```sh
make          # build MarqueeConsole.exe
make run      # build and run in the current console
```

Or launch it in a correctly sized window of its own:

```sh
make run-window
```

## Make commands

| Command | What it does |
| --- | --- |
| `make` | Compiles every `src/*.cpp` into `MarqueeConsole.exe` |
| `make run` | Builds, clears scrollback, then runs in the current console |
| `make run-window` | Opens a new `conhost.exe` window sized to `config.txt` and runs there |
| `make size` | Prints the console size the layout expects (e.g. `100 x 42`) |
| `make clean` | Deletes the built executable |

`run-window` goes through `conhost.exe` on purpose. On Windows 11, `start` hands
new consoles to Windows Terminal, which keeps its own window geometry and ignores
both `mode con` and the program's own resize — leaving the frame clipped.

## Commands

Required by the specification:

| Command | Description |
| --- | --- |
| `help` | Displays the commands and their descriptions |
| `start_marquee` | Starts the marquee animation |
| `stop_marquee` | Stops the marquee animation |
| `set_text <text>` | Sets the text displayed as a marquee |
| `set_speed <ms>` | Sets the marquee animation refresh in milliseconds |
| `exit` | Terminates the console |

Additional tuning, so the other two rates are reachable without recompiling:

| Command | Description |
| --- | --- |
| `set_polling_rate <ms>` | Keyboard polling interval |
| `set_refresh <ms>` | Display refresh interval |
| `status` | Current parameters plus measured FPS and latency |
| `clear` | Clears the output history |

Command names are case-insensitive and surrounding whitespace is ignored.
`set_text` preserves the spacing of its argument and accepts optional
surrounding double quotes.

## Keybinds

The prompt supports shell-style line editing and a 100-entry command history.

| Key | Action |
| --- | --- |
| `Left` / `Right` | Move the cursor one character |
| `Ctrl+Left` / `Ctrl+Right` | Move the cursor one word |
| `Home` / `Ctrl+A` | Jump to the start of the line |
| `PageUp` / `PageDn` | Scroll output panel through history |
| `End` / `Ctrl+E` | Jump to the end of the line |
| `Up` / `Ctrl+P` | Previous command in history |
| `Down` / `Ctrl+N` | Next command in history |
| `Ctrl+U` | Delete from the cursor to the start of the line |
| `Ctrl+K` | Delete from the cursor to the end of the line |
| `Backspace` | Delete the character before the cursor |
| `Delete` | Delete the character after the cursor |
| `Ctrl+C` | Copy the input line to the clipboard |
| `Ctrl+V` | Paste the clipboard at the cursor |
| `Enter` | Submit the line |
| `Esc` | Clear the line |

## Configuration

`config.txt` holds every startup parameter: console size, marquee text and speed,
refresh and polling rates, developer names and version. It is read at startup; if
it is missing, the built-in defaults from `Config.h` are used.

```ini
[console]
console_width       = 100
console_height      = 42
marquee_zone_height = 9

[marquee]
marquee_text     = Hello world in marquee!
marquee_speed_ms = 50

[rates]
refresh_ms = 16   ; raise this to make screen tearing appear
polling_ms = 10   ; raise this to make typing feel laggy
```

> [!NOTE]
> The console window must be exactly `console_width` x `console_height`. The
> layout fills that area, so a smaller window loses rows off the bottom and a
> larger one leaves dead space. `make run-window` sizes it for you.

## Diagnostic flags

```sh
MarqueeConsole.exe --selftest    # unit tests: marquee geometry, parsing, config clamping
MarqueeConsole.exe --dumpframe   # composes one frame and prints it as text
```

Neither needs a console, and `--selftest` exits non-zero on failure.

## Design

Four processes, each with an independent rate, owned by one `Scheduler`:

| Process | Responsibility | Rate |
| --- | --- | --- |
| `keyboard` | Polls the console input buffer | `set_polling_rate` |
| `marquee` | Advances the bouncing text | `set_speed` |
| `display` | Composes and presents a full frame | `set_refresh` |
| `interpreter` | Drains the command queue | event-driven |

Each is a `Process`: its own thread, its own period, and a uniform
start/stop/join lifecycle. Waiting uses a `condition_variable` timed wait against
an absolute deadline, so there is no busy-waiting and no drift, and a process
shuts down immediately regardless of how long its period is.

Rendering is tear-free: a whole frame is composed into an off-screen `CHAR_INFO`
buffer and pushed to the console with a single `WriteConsoleOutputW` call, so a
partially drawn frame can never be visible. The real cursor is hidden and the
caret is drawn into the frame instead.

## Project structure

```
.
├── config.txt            Runtime parameters — no rebuild needed
├── Makefile              Build, run, and window-sizing targets
├── CHANGELOG.md          Version history and bump conventions
└── src/
    ├── main.cpp                Entry point, wiring, shutdown
    ├── Config.*                config.txt parsing, defaults, bounds
    ├── ConsoleScreen.*         Win32 console RAII, raw mode, presentation
    ├── FrameBuffer.*           CHAR_INFO grid and drawing primitives
    ├── Renderer.*              Layout, CSOPESY banner, frame composition
    ├── ConsoleState.*          Shared state and its locking
    ├── MarqueeState.*          Bounce geometry (pure logic, unit-tested)
    ├── CommandParser.*         Tokenising and validation (pure logic, tested)
    ├── CommandInterpreter.*    Command dispatch and handlers
    ├── Process.*               Periodic worker base class
    ├── Scheduler.*             Process lifecycle ownership
    ├── MarqueeProcess.*        Animation tick
    ├── DisplayProcess.*        Frame composition and presentation
    ├── KeyboardProcess.*       Input polling
    ├── InterpreterProcess.*    Command queue drain
    ├── Metrics.*               FPS and latency instrumentation
    ├── SelfTest.*              Unit tests (--selftest)
    └── Diagnostics.*           Headless frame dump (--dumpframe)
```

`main()` lives in `src/main.cpp`. It loads `config.txt`, takes over the console,
creates the four processes, hands them to the `Scheduler`, and waits for the
shutdown signal.

## Developers

CAMPO, Enzo · GARCIA, Andrea · MORGAN, Jack · TENORIO, Jeroen
