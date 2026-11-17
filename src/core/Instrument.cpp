#include "Instrument.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <stdexcept>

namespace kmidi {

const char* kindName(ParamKind kind) {
    switch (kind) {
        case ParamKind::CC: return "CC";
        case ParamKind::RPN: return "RPN";
        case ParamKind::NRPN: return "NRPN";
    }
    return "";
}

static bool isValidUtf8(const std::string& s) {
    size_t i = 0, n = s.size();
    while (i < n) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        int extra = c < 0x80 ? 0 : (c >> 5) == 6 ? 1 : (c >> 4) == 14 ? 2 : (c >> 3) == 30 ? 3 : -1;
        if (extra < 0 || i + extra >= n) return false;
        for (int k = 1; k <= extra; ++k)
            if ((static_cast<unsigned char>(s[i + k]) >> 6) != 2) return false;
        i += extra + 1;
    }
    return true;
}

static void appendUtf8(std::string& out, unsigned code) {
    if (code < 0x80) {
        out += static_cast<char>(code);
    } else if (code < 0x800) {
        out += static_cast<char>(0xC0 | (code >> 6));
        out += static_cast<char>(0x80 | (code & 0x3F));
    } else {
        out += static_cast<char>(0xE0 | (code >> 12));
        out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (code & 0x3F));
    }
}

std::string readTextFile(const std::string& utf8Path) {
    std::ifstream in(std::filesystem::path(std::u8string(utf8Path.begin(), utf8Path.end())), std::ios::binary);
    if (!in) throw std::runtime_error("Can't open " + utf8Path);
    std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.rfind("\xEF\xBB\xBF", 0) == 0) bytes.erase(0, 3);
    if (isValidUtf8(bytes)) return bytes;

    // Windows-1252's characters 0x80-0x9F; the rest of the upper half is the same as Latin-1
    static const unsigned short cp1252[32] = {
        0x20AC, 0x81, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x8D,
        0x017D, 0x8F, 0x90, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A,
        0x0153, 0x9D, 0x017E, 0x0178};
    std::string out;
    out.reserve(bytes.size() + bytes.size() / 8);
    for (char ch : bytes) {
        unsigned char c = static_cast<unsigned char>(ch);
        appendUtf8(out, c >= 0x80 && c < 0xA0 ? cp1252[c - 0x80] : c);
    }
    return out;
}

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string trim(const std::string& s) {
    const char* space = " \t\r\n";
    size_t first = s.find_first_not_of(space);
    if (first == std::string::npos) return {};
    return s.substr(first, s.find_last_not_of(space) - first + 1);
}

Preset makePreset(BankSelect method, int bank, const std::string& bankName, int program, const std::string& name) {
    Preset preset;
    preset.bankName = bankName;
    preset.program = program;
    preset.name = name;
    if (bank >= 0) {
        switch (method) {
            case BankSelect::CC0_CC32: preset.bankMsb = (bank >> 7) & 127; preset.bankLsb = bank & 127; break;
            case BankSelect::CC0: preset.bankMsb = bank & 127; break;
            case BankSelect::CC32: preset.bankLsb = bank & 127; break;
            case BankSelect::ProgramOnly: break;
        }
    }
    return preset;
}

const Parameter* Instrument::find(ParamRef ref) const {
    for (const Parameter& p : parameters)
        if (p.ref() == ref) return &p;
    return nullptr;
}

const Parameter* Instrument::findByName(const std::string& parameterName) const {
    std::string wanted = lower(trim(parameterName));
    for (const Parameter& p : parameters)
        if (lower(trim(p.name)) == wanted) return &p;
    return nullptr;
}

bool isToggleName(const std::string& name) {
    static const std::regex pattern(
        R"(\bon\s*[/\-]\s*off\b|\boff\s*[/\-]\s*on\b|\bswitch\b|\bsw\b|\btoggle\b|\b64\s*-\s*127\s*=\s*on\b|\bon\s*=\s*127\b)",
        std::regex::icase);
    return std::regex_search(name, pattern);
}

bool isUnassignedName(const std::string& name) {
    static const std::regex pattern(
        R"(^\s*(\*\*\*.*||(\d+\s*[-.:=]?\s*)?(undefined|unused|not used|reserved|res|none|n/a|-+)\s*|.*\((unused|not used[^)]*)\)\s*)$)",
        std::regex::icase);
    return std::regex_match(name, pattern);
}

bool isToggle(const Parameter& parameter) {
    return isToggleName(parameter.name) || (parameter.minimum == 0 && parameter.maximum == 1);
}

Instrument resolveVirtual(const Instrument& virtualInstrument, const Instrument& base,
                          std::vector<std::string>* unknownMacros) {
    Instrument result = base;
    result.name = virtualInstrument.name;
    result.source = virtualInstrument.source;
    result.basedOn.clear();
    result.macros.clear();
    for (const auto& [encoder, name] : virtualInstrument.macros) {
        if (base.findByName(name))
            result.macros[encoder] = name;
        else if (unknownMacros)
            unknownMacros->push_back(name);
    }
    return result;
}

}  // namespace kmidi
