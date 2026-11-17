#include "TomlLoader.h"

#include <filesystem>
#include <map>
#include <stdexcept>

#include <toml++/toml.hpp>

namespace kmidi {

namespace {

std::filesystem::path toPath(const std::string& utf8Path) {
    return std::filesystem::path(std::u8string(utf8Path.begin(), utf8Path.end()));
}

toml::table parseFile(const std::string& utf8Path) {
    // Read as text first, so a Windows-1252 file still parses
    std::string text = readTextFile(utf8Path);
    try {
        return toml::parse(text, utf8Path);
    } catch (const toml::parse_error& e) {
        std::string message(e.description());
        const auto& where = e.source().begin;
        throw std::runtime_error(utf8Path + " line " + std::to_string(where.line) + ": " + message);
    }
}

int toNumber(const std::string& key, const std::string& where) {
    try {
        size_t used = 0;
        int number = std::stoi(key, &used);
        if (used == key.size()) return number;
    } catch (const std::exception&) {
    }
    throw std::runtime_error(where + ": '" + key + "' isn't a number");
}

int integerValue(const toml::node& node, const std::string& where) {
    if (auto value = node.value<int64_t>()) return static_cast<int>(*value);
    throw std::runtime_error(where + ": expected a whole number");
}

std::map<int, std::string> valueNames(const toml::table& table, const std::string& where) {
    std::map<int, std::string> names;
    for (const auto& [key, value] : table)
        names[toNumber(std::string(key.str()), where)] = value.value_or(std::string());
    return names;
}

}  // namespace

std::string tomlInstrumentName(const std::string& utf8Path) {
    try {
        toml::table data = parseFile(utf8Path);
        return data["instrument"]["name"].value_or(std::string());
    } catch (const std::exception&) {
        return {};
    }
}

Instrument loadTomlInstrument(const std::string& utf8Path) {
    toml::table data = parseFile(utf8Path);
    const toml::table* header = data["instrument"].as_table();
    if (!header || !(*header)["name"].is_string()) throw std::runtime_error(utf8Path + ": missing [instrument] name");

    Instrument instrument;
    instrument.name = (*header)["name"].value_or(std::string());
    auto stem = toPath(utf8Path).stem().u8string();
    instrument.source.assign(stem.begin(), stem.end());

    if (auto basedOn = (*header)["based_on"].value<std::string>()) {
        instrument.basedOn = *basedOn;
        if (const toml::table* macros = data["macros"].as_table())
            for (const auto& [key, value] : *macros) {
                int encoder = toNumber(std::string(key.str()), utf8Path + ": [macros]");
                if (encoder < 0 || !value.is_string())
                    throw std::runtime_error(utf8Path + ": [macros] must number encoders from 0 and name "
                                             "parameters, e.g. 0 = \"Cutoff\"");
                instrument.macros[encoder] = value.value_or(std::string());
            }
        return instrument;
    }

    // Shared value names, which parameters use by naming the list in values
    std::map<std::string, std::map<int, std::string>> valueLists;
    if (const toml::table* lists = data["value_lists"].as_table())
        for (const auto& [listName, table] : *lists)
            if (const toml::table* t = table.as_table())
                valueLists[std::string(listName.str())] =
                    valueNames(*t, utf8Path + ": [value_lists." + std::string(listName.str()) + "]");

    const std::pair<const char*, ParamKind> tables[] = {
        {"controllers", ParamKind::CC}, {"rpns", ParamKind::RPN}, {"nrpns", ParamKind::NRPN}};
    for (const auto& [tableName, kind] : tables) {
        const toml::table* table = data[tableName].as_table();
        if (!table) continue;
        std::map<int, Parameter> parameters;
        for (const auto& [key, entry] : *table) {
            std::string where = utf8Path + ": [" + tableName + "] " + std::string(key.str());
            Parameter p;
            p.kind = kind;
            p.number = toNumber(std::string(key.str()), where);
            if (const toml::table* details = entry.as_table()) {
                if (!(*details)["name"].is_string()) throw std::runtime_error(where + " has no name");
                p.name = (*details)["name"].value_or(std::string());
                if (const toml::array* range = (*details)["range"].as_array()) {
                    if (range->size() != 2) throw std::runtime_error(where + ": range must be [minimum, maximum]");
                    p.minimum = integerValue(*range->get(0), where);
                    p.maximum = integerValue(*range->get(1), where);
                    if (p.minimum > p.maximum)
                        throw std::runtime_error(where + ": range minimum is above its maximum");
                }
                auto values = (*details)["values"];
                if (auto listName = values.value<std::string>()) {
                    auto it = valueLists.find(*listName);
                    if (it == valueLists.end()) throw std::runtime_error(where + ": no [value_lists." + *listName + "]");
                    p.valueNames = it->second;
                } else if (const toml::table* names = values.as_table()) {
                    p.valueNames = valueNames(*names, where + " values");
                }
            } else {
                p.name = entry.value_or(std::string());
            }
            if (kind == ParamKind::CC && (p.number < 0 || p.number > 127)) continue;
            parameters[p.number] = std::move(p);
        }
        for (auto& [number, p] : parameters) instrument.parameters.push_back(std::move(p));
    }

    // Presets
    static const std::map<std::string, BankSelect> bankSelectNames = {
        {"cc0_cc32", BankSelect::CC0_CC32}, {"cc0", BankSelect::CC0}, {"cc32", BankSelect::CC32},
        {"program_only", BankSelect::ProgramOnly}};
    BankSelect bankSelect = BankSelect::CC0_CC32;
    auto it = bankSelectNames.find(lower((*header)["bank_select"].value_or(std::string("cc0_cc32"))));
    if (it != bankSelectNames.end()) bankSelect = it->second;

    auto addPresets = [&](const toml::table* presets, int bank, const std::string& bankName) {
        if (!presets) return;
        std::map<int, std::string> byProgram;
        for (const auto& [key, value] : *presets)
            byProgram[toNumber(std::string(key.str()), utf8Path + ": presets")] = value.value_or(std::string());
        for (const auto& [program, name] : byProgram)
            if (program >= 0 && program < 128)
                instrument.presets.push_back(makePreset(bankSelect, bank, bankName, program, name));
    };
    if (const toml::array* banks = data["banks"].as_array()) {
        std::map<int, const toml::table*> byNumber;
        for (const toml::node& bank : *banks)
            if (const toml::table* t = bank.as_table()) {
                auto number = (*t)["number"].value<int64_t>();
                if (!number) throw std::runtime_error(utf8Path + ": a [[banks]] entry has no number");
                byNumber[static_cast<int>(*number)] = t;
            }
        for (const auto& [number, bank] : byNumber)
            addPresets((*bank)["presets"].as_table(), number,
                       (*bank)["name"].value_or("Bank " + std::to_string(number)));
    }
    addPresets(data["presets"].as_table(), -1, "");
    return instrument;
}

}  // namespace kmidi
