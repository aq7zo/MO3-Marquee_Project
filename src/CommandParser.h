#pragma once

#include <string>
#include <vector>

namespace mc {

enum class CommandId {
    // The six commands required by the specification.
    Help,
    StartMarquee,
    StopMarquee,
    SetText,
    SetSpeed,
    Exit,
    // Tuning commands. The quiz forbids recompiling, so the polling and refresh
    // rates must be reachable at runtime too.
    SetPollingRate,
    SetRefresh,
    Status,
    Clear,
    // Sentinels.
    Empty,
    Unknown,
};

struct CommandInfo {
    CommandId id;
    const wchar_t* name;
    const wchar_t* usage;
    const wchar_t* description;
    bool requiredBySpec;
};

// Single source of truth: the parser resolves names through it and `help`
// renders from it, so the two can never drift apart.
const std::vector<CommandInfo>& commandCatalog();

const CommandInfo* findCommand(CommandId id);

struct ParsedCommand {
    CommandId id = CommandId::Empty;
    std::wstring name;              // as typed, for error messages
    std::wstring argRaw;            // everything after the name, trimmed, spaces preserved
    std::vector<std::wstring> args; // argRaw split on whitespace
};

enum class NumberParse {
    Ok,
    Empty,
    NotANumber,
    Negative,
    OutOfRange,
};

namespace parser {

std::wstring trim(std::wstring_view s);

// Command names are matched case-insensitively, following the PowerShell /
// Windows shell convention the specification points to as the design reference.
ParsedCommand parse(std::wstring_view line);

// Strips one layer of matching surrounding double quotes, so both
//   set_text Hello world
//   set_text "Hello world"
// yield the same marquee text.
std::wstring stripSurroundingQuotes(std::wstring_view s);

// Strict: the whole string must be an integer. Rejects trailing junk ("12abc"),
// empty input and anything outside int range, and reports negatives distinctly
// so the caller can give a precise message.
NumberParse parseInt(std::wstring_view s, int& out);

}  // namespace parser

}  // namespace mc
