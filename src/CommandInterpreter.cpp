#include "CommandInterpreter.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace mc {

namespace {

std::wstring formatOne(double value) {
    std::wostringstream out;
    out << std::fixed << std::setprecision(1) << value;
    return out.str();
}

std::wstring toWide(const std::string& s) { return std::wstring(s.begin(), s.end()); }

}  // namespace

CommandInterpreter::CommandInterpreter(ConsoleState& state) : state_(state) {}

void CommandInterpreter::attach(Scheduler* scheduler, ProcessHandles handles) {
    scheduler_ = scheduler;
    handles_ = handles;
}

void CommandInterpreter::printWelcome(bool configFileFound) {
    state_.addOutput(L"CSOPESY Marquee Console ready.");
    state_.addOutput(configFileFound ? L"Loaded parameters from config.ini."
                                     : L"No config.ini found - using built-in defaults.");
    state_.addOutput(L"Type 'help' to see the available commands.");
}

void CommandInterpreter::execute(const std::wstring& line) {
    const ParsedCommand cmd = parser::parse(line);
    if (cmd.id == CommandId::Empty) return;

    // Echo first so the output history reads like a real shell transcript.
    state_.addOutput(L"> " + parser::trim(line), true);

    switch (cmd.id) {
        case CommandId::Help:
            handleHelp();
            break;
        case CommandId::StartMarquee:
            handleStartMarquee();
            break;
        case CommandId::StopMarquee:
            handleStopMarquee();
            break;
        case CommandId::SetText:
            handleSetText(cmd);
            break;
        case CommandId::SetSpeed:
            applyRate(cmd, L"Marquee speed", limits::kMinSpeedMs, limits::kMaxSpeedMs,
                      state_.marqueeSpeedMs, handles_.marquee);
            break;
        case CommandId::SetRefresh:
            applyRate(cmd, L"Display refresh", limits::kMinRefreshMs, limits::kMaxRefreshMs,
                      state_.refreshMs, handles_.display);
            break;
        case CommandId::SetPollingRate:
            applyRate(cmd, L"Keyboard polling", limits::kMinPollingMs, limits::kMaxPollingMs,
                      state_.pollingMs, handles_.keyboard);
            break;
        case CommandId::Status:
            handleStatus();
            break;
        case CommandId::Clear:
            state_.clearOutput();
            break;
        case CommandId::Exit:
            handleExit();
            break;
        case CommandId::Unknown:
        default:
            handleUnknown(cmd);
            break;
    }
}

void CommandInterpreter::handleHelp() {
    state_.addOutput(L"Available commands:");
    for (const CommandInfo& info : commandCatalog()) {
        if (!info.requiredBySpec) continue;
        std::wstring line = L"  ";
        line += info.usage;
        line.resize(std::max<size_t>(line.size(), 26), L' ');
        line += L"- ";
        line += info.description;
        state_.addOutput(line);
    }

    state_.addOutput(L"Additional tuning commands:");
    for (const CommandInfo& info : commandCatalog()) {
        if (info.requiredBySpec) continue;
        std::wstring line = L"  ";
        line += info.usage;
        line.resize(std::max<size_t>(line.size(), 26), L' ');
        line += L"- ";
        line += info.description;
        state_.addOutput(line);
    }
}

void CommandInterpreter::handleStartMarquee() {
    if (state_.marqueeRunning.exchange(true)) {
        state_.addOutput(L"Marquee is already running.");
    } else {
        state_.addOutput(L"Marquee animation started.");
    }
}

void CommandInterpreter::handleStopMarquee() {
    if (!state_.marqueeRunning.exchange(false)) {
        state_.addOutput(L"Marquee is already stopped.");
    } else {
        state_.addOutput(L"Marquee animation stopped.");
    }
}

void CommandInterpreter::handleSetText(const ParsedCommand& cmd) {
    if (cmd.argRaw.empty()) {
        state_.addOutput(L"set_text requires text. Usage: set_text <text>");
        return;
    }

    std::wstring text = parser::stripSurroundingQuotes(cmd.argRaw);
    if (parser::trim(text).empty()) {
        state_.addOutput(L"set_text requires non-blank text. Usage: set_text <text>");
        return;
    }

    if (static_cast<int>(text.size()) > limits::kMaxTextLength) {
        text.resize(static_cast<size_t>(limits::kMaxTextLength));
        state_.addOutput(L"Text truncated to " + std::to_wstring(limits::kMaxTextLength) +
                         L" characters.");
    }

    state_.setMarqueeText(text);
    state_.addOutput(L"Marquee text set to: \"" + text + L"\"");
}

void CommandInterpreter::applyRate(const ParsedCommand& cmd,
                                   const wchar_t* label,
                                   int minMs,
                                   int maxMs,
                                   std::atomic<int>& slot,
                                   Process* target) {
    const CommandInfo* info = findCommand(cmd.id);
    const std::wstring usage = info ? info->usage : cmd.name;

    if (cmd.args.empty()) {
        state_.addOutput(std::wstring(label) + L" requires a value. Usage: " + usage);
        return;
    }
    if (cmd.args.size() > 1) {
        state_.addOutput(std::wstring(label) + L" takes exactly one value. Usage: " + usage);
        return;
    }

    int value = 0;
    switch (parser::parseInt(cmd.args[0], value)) {
        case NumberParse::Ok:
            break;
        case NumberParse::Negative:
            state_.addOutput(std::wstring(label) + L" must be a positive number of milliseconds.");
            return;
        case NumberParse::OutOfRange:
            state_.addOutput(std::wstring(label) + L" value is too large. Valid range: " +
                             std::to_wstring(minMs) + L"-" + std::to_wstring(maxMs) + L" ms.");
            return;
        case NumberParse::Empty:
        case NumberParse::NotANumber:
        default:
            state_.addOutput(L"'" + cmd.args[0] + L"' is not a valid number of milliseconds.");
            return;
    }

    if (value < minMs || value > maxMs) {
        const int clamped = std::clamp(value, minMs, maxMs);
        state_.addOutput(std::wstring(label) + L" must be between " + std::to_wstring(minMs) +
                         L" and " + std::to_wstring(maxMs) + L" ms; using " +
                         std::to_wstring(clamped) + L" ms.");
        value = clamped;
    }

    slot.store(value);
    if (target) target->setPeriodMs(value);
    state_.addOutput(std::wstring(label) + L" set to " + std::to_wstring(value) + L" ms.");
}

void CommandInterpreter::handleStatus() {
    state_.addOutput(L"--- Build ---");
    state_.addOutput(L"  version          : v" + state_.version);

    state_.addOutput(L"--- Parameters ---");
    state_.addOutput(L"  marquee speed    : " + std::to_wstring(state_.marqueeSpeedMs.load()) +
                     L" ms  (set_speed)");
    state_.addOutput(L"  display refresh  : " + std::to_wstring(state_.refreshMs.load()) +
                     L" ms  (set_refresh)");
    state_.addOutput(L"  keyboard polling : " + std::to_wstring(state_.pollingMs.load()) +
                     L" ms  (set_polling_rate)");
    state_.addOutput(L"  marquee state    : " +
                     std::wstring(state_.marqueeRunning.load() ? L"running" : L"stopped"));
    state_.addOutput(L"  marquee text     : \"" + state_.marqueeText() + L"\"");

    const Metrics& m = state_.metrics;
    state_.addOutput(L"--- Measured ---");
    state_.addOutput(L"  frames rendered  : " + std::to_wstring(m.frameCount()));
    state_.addOutput(L"  actual FPS       : " + formatOne(m.fps()));
    state_.addOutput(L"  frame interval   : " + formatOne(m.avgFrameMs()) + L" ms");
    state_.addOutput(L"  frame compose    : " + formatOne(m.avgComposeMs()) + L" ms");
    state_.addOutput(L"  poll interval    : " + formatOne(m.avgPollMs()) + L" ms");
    state_.addOutput(L"  input latency    : " + formatOne(m.lastInputLatencyMs()) +
                     L" ms (peak " + formatOne(m.maxInputLatencyMs()) + L" ms)");

    if (scheduler_) {
        state_.addOutput(L"--- Processes ---");
        for (const Scheduler::ProcessStatus& ps : scheduler_->status()) {
            state_.addOutput(L"  " + toWide(ps.name) + L" @ " + std::to_wstring(ps.periodMs) +
                             L" ms");
        }
    }
}

void CommandInterpreter::handleExit() {
    state_.addOutput(L"Terminating console...");
    state_.requestShutdown();
}

void CommandInterpreter::handleUnknown(const ParsedCommand& cmd) {
    state_.addOutput(L"Unknown command: '" + cmd.name + L"'. Type 'help' for a list of commands.");
}

}  // namespace mc
