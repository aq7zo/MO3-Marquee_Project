#pragma once

namespace mc {

// Composes one frame exactly as the display process would and writes it to
// stdout as plain text.
//
// Reachable as `MarqueeConsole.exe --dumpframe`. It exercises the whole render
// path - layout, banner, marquee placement, output history, prompt - without a
// console attached, so the screen can be checked from a script or a pty where
// the real UI cannot run.
int runFrameDump();

}  // namespace mc
