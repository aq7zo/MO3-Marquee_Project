# Changelog — MO3 Marquee Console

All notable changes to this project are recorded here. This file is the source of
truth for versioning; there is no git history yet, so **a version is not considered
released until it appears below and has a matching folder in `versions/`.**

Format follows [Keep a Changelog](https://keepachangelog.com/). Versions use
`MAJOR.MINOR.PATCH`.

## Conventions

- Bump **PATCH** for fixes and doc changes, **MINOR** for new commands or features,
  **MAJOR** only if the command surface breaks.
- On every bump: update `[about] version` in `config.ini`, add the entry here, and
  take a snapshot into `versions/vX.Y.Z_YYYY-MM-DD/`.
- A version is only recorded after `--selftest` passes and `--dumpframe` shows the
  welcome screen intact.

---

## [Unreleased]

### Added
- **Delete** removes the character after the caret.
- **Ctrl+C / Ctrl+V** copy the input line to, and paste from, the Windows
  clipboard. Pasted line breaks and control characters are dropped.

### Changed
- Ctrl+C no longer exits; use `exit`.

Nothing yet. See `.context/todoList_v0.1.md` for what is queued — **P0-1**
(`README.txt` name) and **P0-3** (`config.ini` version date) still block
submission.

---

## [1.1.0] — 2026-09-20

Prompt editing catches up with what a shell user expects, the window can no
longer be resized out from under the fixed layout, and the real group names
replace the specification's mockup ones.

### Added
- **Line editing at the prompt.** Left/Right, Ctrl+Left/Right by word, Home/End
  (and their Ctrl+A / Ctrl+E chords), Ctrl+U and Ctrl+K to cut to the start or
  end of the line. Typing and backspace act at the caret rather than only at the
  end. Previously the only edit available was backspace, so a typo in a long
  `set_text` meant retyping the whole line — on the quiz video, where every
  keystroke is on camera.
- **Command history.** Up/Down (and Ctrl+P / Ctrl+N) walk a 100-entry history.
  Consecutive duplicates take one slot, and the half-typed line is held aside so
  walking back down returns it.
- **`ConsoleState::inputSnapshot()`** replaces `inputLine()`: the text and the
  caret index are read under one lock. With a movable caret, two separate
  accessors could interleave with a keystroke and draw the caret past the end of
  the text.

### Changed
- **The console window no longer resizes.** `ConsoleScreen::lockWindowSize()`
  drops `WS_THICKFRAME` and `WS_MAXIMIZEBOX` before the buffer is sized; Windows
  greys the matching system-menu entries itself. conhost resizes its window
  without telling the buffer to follow, so a border drag could only clip the
  frame or leave dead space. The original style is restored on exit, because
  `make run` locks the user's own console window, not a throwaway one.
- **The screen is blanked on startup and on exit.** A run never starts under
  leftover shell output, and `exit` leaves the terminal clean instead of showing
  the last frame.
- **Prompt scrolling follows the caret.** The visible window used to track the
  tail of the line, which is wrong once the caret can sit anywhere: editing the
  start of a long line now scrolls back to it.
- **`developers`** in `config.ini` is the real group — `CAMPO, Enzo;
  GARCIA, Andrea; MORGAN, Jack; TENORIO, Jeroen` — closing **P0-2**. The header
  grows one row per name, so it is 16 rows rather than 14 and every zone below
  it shifts down by two.
- **`Group developer:`** → **`Group developers:`** on the welcome screen.

### Verified at this version
- `g++ -std=c++17 -O2 -Wall src/*.cpp -o MarqueeConsole.exe -static -pthread` —
  clean, no warnings
- `--selftest` — **104 checks, 0 failures**, exit 0 (81 at v1.0.0; the 23 new
  ones cover cursor motion, the kills, and history including the duplicate and
  draft cases)
- `--dumpframe` — `header 16 | marquee 16..24 | output 25..38 | prompt 40`, and
  `v1.1.0` at the right edge of the version-date row

### Known gaps (unchanged from 1.0.0 unless noted)
- `README.txt:8` still reads `<<< FILL IN: Your full name >>>` — **P0-1**
- `config.ini` `version_date` is still a placeholder — **P0-3**
- Screen-tearing and typing-lag thresholds not yet measured on this hardware —
  **P1-2**, **P1-3**
- PPT and demo video not started — **P2-1**, **P2-2**
- ~~mockup developer names~~ — closed by this release

---

## [1.0.0] — 2026-09-15

First tracked version. The project was already code-complete and passing its tests
at this point; this release is where version tracking begins.

### Added
- **Version tracking infrastructure** — this changelog, the `versions/` snapshot
  folder, and the `.context/` project convention.
- **`version` key** in `config.ini` `[about]`, parsed by `Config::loadOrDefault`
  and bounded by `limits::kMaxVersionLength` (16 chars). Absent key falls back to
  `1.0.0`, matching how every other config key degrades.
- Build version rendered right-aligned on the version-date row of the welcome
  screen, and reported by `status` under a new `--- Build ---` heading. Display
  only — it identifies which snapshot is running on the quiz video, where the code
  cannot be inspected.

### Notes on the existing implementation (documented here for the first time)

- **Four-process architecture** owned by one `Scheduler`, each worker with an
  independent, runtime-tunable period:

  | Process | Default | Tuned by |
  |---|---|---|
  | `MarqueeProcess` | 50 ms | `set_speed` |
  | `DisplayProcess` | 16 ms (~60 FPS) | `set_refresh` |
  | `KeyboardProcess` | 10 ms | `set_polling_rate` |
  | `InterpreterProcess` | event-driven | — |

  Keeping these three rates decoupled is what makes the required
  "refresh rate vs. polling rate" analysis measurable at all.

- **Ten commands.** Six required by the spec (`help`, `start_marquee`,
  `stop_marquee`, `set_text`, `set_speed`, `exit`) plus four tuning commands
  (`set_polling_rate`, `set_refresh`, `status`, `clear`) added because the quiz
  forbids recompiling — every tunable has to be reachable at runtime.

- **Tear-free rendering.** A whole frame is composed into an off-screen
  `CHAR_INFO` buffer and pushed with a single `WriteConsoleOutputW`, so a
  half-drawn frame can never be visible. The real cursor is hidden and the caret
  is drawn into the frame.

- **No busy-waiting.** `Process` waits on a `condition_variable` against an
  absolute deadline, so CPU stays near idle while animating, there is no
  cumulative drift, and a process with a 60-second period still stops instantly.

- **Self-test:** `--selftest` runs 81 checks across marquee geometry, command
  parsing, config clamping, and live scheduler/process threading. `--dumpframe`
  renders one frame as text for headless layout checks.

### Verified at this version
- `msbuild MarqueeConsole.sln -p:Configuration=Debug -p:Platform=x64` — clean, no warnings
- `--selftest` — **81 checks, 0 failures**, exit 0
- `--dumpframe` — welcome screen intact; layout unchanged from pre-1.0.0
  (`header 14 | marquee 14..22 | output 23..38 | prompt 40`). The only pixel
  difference introduced by this release is the `v1.0.0` string at the right edge
  of row 14.

### Known gaps (do not ship without these)
- `README.txt:8` still reads `<<< FILL IN: Your full name >>>` — **P0-1**
- `config.ini` `developers` still holds the specification's own mockup names
  (`De La Cruz, Juan; Santos, Alex`) rather than the real group — **P0-2**
- `config.ini` `version_date` is a placeholder — **P0-3**
- Screen-tearing and typing-lag thresholds not yet measured on this hardware; the
  technical report requires both — **P1-2**, **P1-3**
- PPT and demo video not started — **P2-1**, **P2-2**
