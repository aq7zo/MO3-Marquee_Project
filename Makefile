# Windows-only project (Win32 console APIs), so pin the recipe shell to cmd.exe
# rather than guessing: GNU Make otherwise runs recipes through sh.exe when one
# happens to be on PATH, and the two disagree on every command below.
SHELL       := cmd.exe
.SHELLFLAGS := /c

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall
LDFLAGS  ?= -static -pthread
TARGET   := MarqueeConsole.exe
SRCS     := $(wildcard src/*.cpp)
TITLE    := MO3 - Marquee Console

# The window has to be exactly console_width x console_height: Renderer lays the
# header, marquee zone, output zone and prompt out to fill that, so anything
# smaller loses rows off the bottom and anything larger leaves dead space.
# Read from config.ini so editing it is still enough - no rebuild, no edit here.
# The fallbacks match Config.h, for when config.ini is absent.
WINDIR  := $(if $(SystemRoot),$(SystemRoot),C:\Windows)
CONHOST := $(if $(wildcard $(WINDIR)\Sysnative\conhost.exe),$(WINDIR)\Sysnative\conhost.exe,$(WINDIR)\System32\conhost.exe)

CFG_COLS := $(shell for /f "tokens=3" %%a in ('findstr /b console_width config.ini') do @echo %%a)
CFG_ROWS := $(shell for /f "tokens=3" %%a in ('findstr /b console_height config.ini') do @echo %%a)
COLS     := $(if $(CFG_COLS),$(CFG_COLS),100)
ROWS     := $(if $(CFG_ROWS),$(CFG_ROWS),42)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# ConsoleScreen blanks the console on startup and again on exit, so this only
# has to wipe scrollback left behind by earlier commands.
run: $(TARGET)
	@cls
	.\$(TARGET)

# A new console window of its own, opened at the size the display needs so it is
# never resized out from under the first frame. `mode con` also drops the
# scrollback, and full screen still works (Alt+Enter).
#
# Launched through conhost.exe on purpose: on Windows 11 `start` hands the new
# console to whatever DelegationTerminal is set to, normally Windows Terminal,
# which keeps its own window geometry and ignores both `mode con` and the
# SetConsoleWindowInfo call in ConsoleScreen - the buffer becomes COLS x ROWS
# but the window stays at the profile's default size, so the frame is clipped.
# conhost.exe is the classic host, always in System32, and it does size the
# window to the buffer.
#
# By full path, not by PATH: make.exe is often 32-bit (the Chocolatey one
# is), and under WOW64 \System32 redirects to \SysWOW64, which ships no
# conhost.exe - `start conhost.exe` then fails with "cannot find the file".
# \Sysnative is the un-redirected view and only exists for 32-bit processes,
# so probe for it and fall back to \System32 for a 64-bit make.
run-window: $(TARGET)
	start "$(TITLE)" "$(CONHOST)" cmd /c mode con: cols=$(COLS) lines=$(ROWS) ^& .\$(TARGET)

size:
	@echo $(COLS) x $(ROWS)

clean:
	if exist $(TARGET) del /q $(TARGET)

.PHONY: run run-window size clean
