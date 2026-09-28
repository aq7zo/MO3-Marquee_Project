#include "Config.h"


#ifndef NOMINMAX
#define NOMINMAX
#endif


#include <windows.h>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace mc {

namespace {

std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

std::wstring toWide(const std::string& utf8) {
    if (utf8.empty()) return {};
    const int needed =
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    if (needed <= 0) return {};
    std::wstring out(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), out.data(), needed);
    return out;
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(::tolower(c));
    });
    return s;
}

// Only overwrites the target when the value parses as an integer, so a typo in
// config.txt degrades to the default instead of zeroing a rate.
void assignInt(const std::string& value, int& target) {
    try {
        size_t consumed = 0;
        const int parsed = std::stoi(value, &consumed);
        if (consumed == value.size()) target = parsed;
    } catch (...) {
        // Leave the default in place.
    }
}

void assignBool(const std::string& value, bool& target) {
    const std::string v = lower(value);
    if (v == "1" || v == "true" || v == "yes" || v == "on") target = true;
    else if (v == "0" || v == "false" || v == "no" || v == "off") target = false;
}

// Developer names contain commas ("De La Cruz, Juan"), so the list separator is
// a semicolon.
std::vector<std::wstring> splitDevelopers(const std::string& value) {
    std::vector<std::wstring> out;
    std::stringstream ss(value);
    std::string item;
    while (std::getline(ss, item, ';')) {
        const std::string cleaned = trim(item);
        if (!cleaned.empty()) out.push_back(toWide(cleaned));
    }
    return out;
}

}  // namespace

void Config::clampAll() {
    consoleWidth = std::clamp(consoleWidth, 60, 240);
    consoleHeight = std::clamp(consoleHeight, 20, 80);
    marqueeSpeedMs = std::clamp(marqueeSpeedMs, limits::kMinSpeedMs, limits::kMaxSpeedMs);
    refreshMs = std::clamp(refreshMs, limits::kMinRefreshMs, limits::kMaxRefreshMs);
    pollingMs = std::clamp(pollingMs, limits::kMinPollingMs, limits::kMaxPollingMs);

    // The marquee zone has to leave room for the header, output and prompt.
    marqueeZoneHeight = std::clamp(marqueeZoneHeight, 3, std::max(3, consoleHeight - 16));

    if (static_cast<int>(marqueeText.size()) > limits::kMaxTextLength) {
        marqueeText.resize(static_cast<size_t>(limits::kMaxTextLength));
    }

    if (static_cast<int>(version.size()) > limits::kMaxVersionLength) {
        version.resize(static_cast<size_t>(limits::kMaxVersionLength));
    }
}

Config Config::loadOrDefault(const std::string& path, bool* fileFound) {
    Config cfg;
    std::ifstream file(path);
    if (fileFound) *fileFound = false;

    if (file) {
        if (fileFound) *fileFound = true;
        std::string line;
        while (std::getline(file, line)) {
            const std::string trimmed = trim(line);
            if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') continue;
            if (trimmed.front() == '[') continue;  // section headers are decorative

            const auto eq = trimmed.find('=');
            if (eq == std::string::npos) continue;

            const std::string key = lower(trim(trimmed.substr(0, eq)));
            const std::string value = trim(trimmed.substr(eq + 1));

            if (key == "console_width") assignInt(value, cfg.consoleWidth);
            else if (key == "console_height") assignInt(value, cfg.consoleHeight);
            else if (key == "marquee_zone_height") assignInt(value, cfg.marqueeZoneHeight);
            else if (key == "marquee_text") cfg.marqueeText = toWide(value);
            else if (key == "marquee_speed_ms") assignInt(value, cfg.marqueeSpeedMs);
            else if (key == "refresh_ms") assignInt(value, cfg.refreshMs);
            else if (key == "polling_ms") assignInt(value, cfg.pollingMs);
            else if (key == "auto_start_marquee") assignBool(value, cfg.autoStartMarquee);
            else if (key == "version_date") cfg.versionDate = toWide(value);
            else if (key == "version") cfg.version = toWide(value);
            else if (key == "developers") {
                auto parsed = splitDevelopers(value);
                if (!parsed.empty()) cfg.developers = std::move(parsed);
            }
        }
    }

    cfg.clampAll();
    return cfg;
}

}  // namespace mc
