#include "InsLoader.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace kmidi {

namespace {

const char* const kPatchNames = "Patch Names";
const char* const kControllerNames = "Controller Names";
const char* const kRpnNames = "RPN Names";
const char* const kNrpnNames = "NRPN Names";
const char* const kInstrumentDefinitions = "Instrument Definitions";

// The standard section names, so that ".patch names" reads as "Patch Names"
std::string standardSectionName(const std::string& name) {
    for (const char* standard : {kPatchNames, "Note Names", kControllerNames, kRpnNames, kNrpnNames,
                                 kInstrumentDefinitions})
        if (lower(name) == lower(standard)) return standard;
    return name;
}

bool isNumber(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
}

std::string sourceName(const std::string& utf8Path) {
    std::filesystem::path p(std::u8string(utf8Path.begin(), utf8Path.end()));
    auto stem = p.stem().u8string();
    return std::string(stem.begin(), stem.end());
}

}  // namespace

const InsLoader::Lists* InsLoader::File::section(const std::string& type) const {
    for (const auto& [name, lists] : sections)
        if (name == type) return &lists;
    return nullptr;
}

void InsLoader::addFile(const std::string& utf8Path) {
    std::string text = readTextFile(utf8Path);
    File file;
    file.source = sourceName(utf8Path);
    Lists* currentSection = nullptr;
    Entries* currentList = nullptr;

    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';') continue;
        if (line[0] == '.') {
            std::string type = standardSectionName(trim(line.substr(1)));
            auto it = std::find_if(file.sections.begin(), file.sections.end(),
                                   [&](const auto& s) { return s.first == type; });
            if (it == file.sections.end()) {
                file.sections.emplace_back(type, Lists{});
                it = file.sections.end() - 1;
            }
            currentSection = &it->second;
            currentList = nullptr;
        } else if (line.front() == '[' && line.back() == ']') {
            if (!currentSection) continue;
            std::string name = trim(line.substr(1, line.size() - 2));
            auto it = std::find_if(currentSection->begin(), currentSection->end(),
                                   [&](const auto& l) { return l.first == name; });
            if (it == currentSection->end()) {
                currentSection->emplace_back(name, Entries{});
                it = currentSection->end() - 1;
            }
            currentList = &it->second;
        } else if (currentList) {
            size_t equals = line.find('=');
            if (equals == std::string::npos) continue;
            std::string key = trim(line.substr(0, equals)), value = trim(line.substr(equals + 1));
            // A repeated key replaces the earlier value, keeping its place
            auto it = std::find_if(currentList->begin(), currentList->end(),
                                   [&](const auto& e) { return e.first == key; });
            if (it != currentList->end())
                it->second = value;
            else
                currentList->emplace_back(key, value);
        }
    }
    // Pointers into the sections are only used while reading, so moving the file is safe
    files_.push_back(std::move(file));
}

std::vector<std::string> InsLoader::instrumentNames(size_t fileIndex) const {
    std::vector<std::string> names;
    if (const Lists* definitions = files_.at(fileIndex).section(kInstrumentDefinitions))
        for (const auto& [name, entries] : *definitions) names.push_back(name);
    return names;
}

Instrument InsLoader::instrument(size_t fileIndex, const std::string& name) const {
    if (const Lists* definitions = files_.at(fileIndex).section(kInstrumentDefinitions))
        for (const auto& [listName, entries] : *definitions)
            if (listName == name) return build(listName, entries, fileIndex);
    throw std::runtime_error("No instrument named " + name);
}

std::vector<Instrument> InsLoader::allInstruments() const {
    std::vector<Instrument> instruments;
    for (size_t i = 0; i < files_.size(); ++i)
        for (const std::string& name : instrumentNames(i)) instruments.push_back(instrument(i, name));
    return instruments;
}

const InsLoader::Entries* InsLoader::findList(const std::string& sectionType, const std::string& name,
                                              size_t fileIndex) const {
    // The named list (ignoring case) from the given file if it has it, or else from any other file
    std::string wanted = lower(name);
    std::vector<size_t> order{fileIndex};
    for (size_t i = 0; i < files_.size(); ++i)
        if (i != fileIndex) order.push_back(i);
    for (size_t i : order)
        if (const Lists* lists = files_[i].section(sectionType))
            for (const auto& [listName, entries] : *lists)
                if (lower(listName) == wanted) return &entries;
    return nullptr;
}

std::vector<std::pair<int, std::string>> InsLoader::resolveList(const std::string& sectionType,
                                                                const std::string& name, size_t fileIndex,
                                                                int depth) const {
    // Cakewalk's built-in patch lists, numbering programs from 0 or 1
    if (sectionType == kPatchNames && (name == "0..127" || name == "1..128")) {
        int first = name == "0..127" ? 0 : 1;
        std::vector<std::pair<int, std::string>> programs;
        for (int p = 0; p < 128; ++p) programs.emplace_back(p, "Program " + std::to_string(p + first));
        return programs;
    }
    // Guards against lists that are BasedOn each other
    if (depth > 32) return {};
    const Entries* entries = findList(sectionType, name, fileIndex);
    if (!entries) return {};

    std::map<int, std::string> resolved;
    for (const auto& [key, value] : *entries)
        if (lower(key) == "basedon")
            for (auto& [number, entryName] : resolveList(sectionType, value, fileIndex, depth + 1))
                resolved[number] = entryName;
    for (const auto& [key, value] : *entries)
        if (isNumber(key) && key.size() < 9) resolved[std::stoi(key)] = value;
    return {resolved.begin(), resolved.end()};
}

Instrument InsLoader::build(const std::string& name, const Entries& entries, size_t fileIndex) const {
    static const std::regex patchKey(R"(^patch\[(\d+|\*)\]$)", std::regex::icase);
    Instrument instrument;
    instrument.name = name;
    instrument.source = files_[fileIndex].source;

    std::vector<std::pair<std::string, std::string>> patchLists;  // bank ("*" for any) -> patch names list
    BankSelect bankSelect = BankSelect::CC0_CC32;
    std::map<std::string, std::string> namedLists;  // "control", "rpn", "nrpn" -> list name
    for (const auto& [key, value] : entries) {
        std::smatch match;
        std::string lowerKey = lower(key);
        if (std::regex_match(key, match, patchKey)) {
            patchLists.emplace_back(match[1].str(), value);
        } else if (lowerKey == "bankselmethod") {
            if (isNumber(value) && value.size() == 1 && value[0] <= '3')
                bankSelect = static_cast<BankSelect>(value[0] - '0');
        } else if (lowerKey == "control" || lowerKey == "rpn" || lowerKey == "nrpn") {
            namedLists[lowerKey] = value;
        }
    }

    auto addParameters = [&](const char* listKey, const char* sectionType, ParamKind kind) {
        auto it = namedLists.find(listKey);
        if (it == namedLists.end()) return;
        for (auto& [number, paramName] : resolveList(sectionType, it->second, fileIndex)) {
            if (kind == ParamKind::CC && number > 127) continue;
            Parameter p;
            p.kind = kind;
            p.number = number;
            p.name = paramName;
            instrument.parameters.push_back(std::move(p));
        }
    };
    addParameters("control", kControllerNames, ParamKind::CC);
    addParameters("rpn", kRpnNames, ParamKind::RPN);
    addParameters("nrpn", kNrpnNames, ParamKind::NRPN);

    // Presets: numbered banks in bank order, then those for any bank. An instrument with no Patch entries, such
    // as General MIDI, uses the patch names list of the same name.
    if (patchLists.empty()) patchLists.emplace_back("*", name);
    std::stable_sort(patchLists.begin(), patchLists.end(), [](const auto& a, const auto& b) {
        auto rank = [](const std::string& bank) { return bank == "*" ? 1LL << 40 : std::stoll(bank); };
        return rank(a.first) < rank(b.first);
    });
    for (const auto& [bank, listName] : patchLists) {
        int bankNumber = bank == "*" ? -1 : (bank.size() < 9 ? std::stoi(bank) : -1);
        for (auto& [program, presetName] : resolveList(kPatchNames, listName, fileIndex))
            if (program < 128)
                instrument.presets.push_back(makePreset(bankSelect, bankNumber, listName, program, presetName));
    }
    return instrument;
}

std::vector<Instrument> loadInsFile(const std::string& utf8Path) {
    InsLoader loader;
    loader.addFile(utf8Path);
    return loader.allInstruments();
}

}  // namespace kmidi
