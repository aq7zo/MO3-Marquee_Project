#include "CommandParser.h"

#include <algorithm>
#include <cwctype>
#include <limits>

namespace mc {

const std::vector<CommandInfo>& commandCatalog() {
    // Descriptions use the specification's own wording for the six required
    // commands, so `help` reads back exactly what the spec asked for.
    static const std::vector<CommandInfo> catalog = {
        {CommandId::Help, L"help", L"help",
         L"displays the commands and its description", true},
        {CommandId::StartMarquee, L"start_marquee", L"start_marquee",
         L"starts the marquee \"animation\"", true},
        {CommandId::StopMarquee, L"stop_marquee", L"stop_marquee",
         L"stops the marquee \"animation\"", true},
        {CommandId::SetText, L"set_text", L"set_text <text>",
         L"accepts a text input and displays it as a marquee", true},
        {CommandId::SetSpeed, L"set_speed", L"set_speed <ms>",
         L"sets the marquee animation refresh in milliseconds", true},
        {CommandId::Exit, L"exit", L"exit",
         L"terminates the console", true},

        {CommandId::SetPollingRate, L"set_polling_rate", L"set_polling_rate <ms>",
         L"sets the keyboard polling interval in milliseconds", false},
        {CommandId::SetRefresh, L"set_refresh", L"set_refresh <ms>",
         L"sets the display refresh interval in milliseconds", false},
        {CommandId::Status, L"status", L"status",
         L"displays current parameters and measured performance", false},
        {CommandId::Clear, L"clear", L"clear",
         L"clears the output history", false},
    };
    return catalog;
}

const CommandInfo* findCommand(CommandId id) {
    for (const CommandInfo& info : commandCatalog()) {
        if (info.id == id) return &info;
    }
    return nullptr;
}

namespace parser {

namespace {

bool isSpace(wchar_t c) { return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n'; }

std::wstring toLower(std::wstring_view s) {
    std::wstring out(s);
    std::transform(out.begin(), out.end(), out.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(std::towlower(c));
    });
    return out;
}

CommandId lookup(const std::wstring& lowerName) {
    for (const CommandInfo& info : commandCatalog()) {
        if (lowerName == info.name) return info.id;
    }
    return CommandId::Unknown;
}

}  // namespace

std::wstring trim(std::wstring_view s) {
    size_t begin = 0;
    size_t end = s.size();
    while (begin < end && isSpace(s[begin])) ++begin;
    while (end > begin && isSpace(s[end - 1])) --end;
    return std::wstring(s.substr(begin, end - begin));
}

ParsedCommand parse(std::wstring_view line) {
    ParsedCommand cmd;

    const std::wstring trimmed = trim(line);
    if (trimmed.empty()) {
        cmd.id = CommandId::Empty;
        return cmd;
    }

    size_t split = 0;
    while (split < trimmed.size() && !isSpace(trimmed[split])) ++split;

    cmd.name = trimmed.substr(0, split);
    cmd.argRaw = trim(std::wstring_view(trimmed).substr(split));
    cmd.id = lookup(toLower(cmd.name));

    // Split argRaw on whitespace for the commands that take discrete arguments.
    // set_text ignores this and uses argRaw verbatim so its spacing survives.
    size_t i = 0;
    while (i < cmd.argRaw.size()) {
        while (i < cmd.argRaw.size() && isSpace(cmd.argRaw[i])) ++i;
        const size_t start = i;
        while (i < cmd.argRaw.size() && !isSpace(cmd.argRaw[i])) ++i;
        if (i > start) cmd.args.push_back(cmd.argRaw.substr(start, i - start));
    }

    return cmd;
}

std::wstring stripSurroundingQuotes(std::wstring_view s) {
    if (s.size() >= 2 && s.front() == L'"' && s.back() == L'"') {
        return std::wstring(s.substr(1, s.size() - 2));
    }
    return std::wstring(s);
}

NumberParse parseInt(std::wstring_view s, int& out) {
    const std::wstring text = trim(s);
    if (text.empty()) return NumberParse::Empty;

    size_t i = 0;
    bool negative = false;
    if (text[i] == L'+' || text[i] == L'-') {
        negative = (text[i] == L'-');
        ++i;
    }
    if (i >= text.size()) return NumberParse::NotANumber;

    long long value = 0;
    for (; i < text.size(); ++i) {
        const wchar_t c = text[i];
        if (c < L'0' || c > L'9') return NumberParse::NotANumber;
        value = value * 10 + (c - L'0');
        // Bail out early so a long digit string cannot overflow the accumulator.
        if (value > static_cast<long long>(std::numeric_limits<int>::max())) {
            return NumberParse::OutOfRange;
        }
    }

    if (negative && value != 0) return NumberParse::Negative;
    out = static_cast<int>(value);  // "-0" is just 0
    return NumberParse::Ok;
}

}  // namespace parser

}  // namespace mc
