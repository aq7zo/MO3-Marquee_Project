#pragma once

namespace mc {

// Runs the unit tests over the I/O-free logic (marquee geometry, command
// parsing, config clamping) and prints a report. Returns 0 when everything
// passed.
//
// Reachable as `MarqueeConsole.exe --selftest`, which means the parsing and
// bounce rules can be verified without a console attached and without a
// separate test project to keep in sync.
int runSelfTest();

}  // namespace mc
