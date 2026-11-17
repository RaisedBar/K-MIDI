#include "Reabank.h"

#include <filesystem>
#include <sstream>

namespace kmidi {

namespace {

// A bank select number (0-127), or -1 for "*" or anything else
int bankPart(const std::string& word) {
    if (word.empty() || word.size() > 3) return -1;
    for (char c : word)
        if (c < '0' || c > '9') return -1;
    int value = std::stoi(word);
    return value <= 127 ? value : -1;
}

}  // namespace

Instrument loadReabank(const std::string& utf8Path) {
    std::string text = readTextFile(utf8Path);
    Instrument instrument;
    auto stem = std::filesystem::path(std::u8string(utf8Path.begin(), utf8Path.end())).stem().u8string();
    instrument.name.assign(stem.begin(), stem.end());
    instrument.source = instrument.name;

    int msb = -1, lsb = -1;
    std::string bankName;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        line = trim(line);
        if (line.empty() || line.rfind("//", 0) == 0) continue;
        std::istringstream words(line);
        std::string first;
        words >> first;
        if (lower(first) == "bank") {
            std::string msbWord, lsbWord;
            words >> msbWord >> lsbWord;
            msb = bankPart(msbWord);
            lsb = bankPart(lsbWord);
            std::getline(words, bankName);
            bankName = trim(bankName);
            continue;
        }
        int program = bankPart(first);
        if (program < 0) continue;
        std::string name;
        std::getline(words, name);
        Preset preset;
        preset.bankMsb = msb;
        preset.bankLsb = lsb;
        preset.bankName = bankName;
        preset.program = program;
        preset.name = trim(name);
        instrument.presets.push_back(std::move(preset));
    }
    return instrument;
}

}  // namespace kmidi
