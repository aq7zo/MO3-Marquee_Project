SHELL := cmd.exe
.SHELLFLAGS := /c
t:
	wt.exe --size 100,42 -d "%CD%" "%CD%\MarqueeConsole.exe"
