// An instrument's parameters, as read from a Cakewalk .ins, Asp .toml or REAPER .reabank file.
#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace kmidi {

enum class ParamKind { CC, RPN, NRPN };

// "CC", "RPN" or "NRPN"
const char* kindName(ParamKind kind);

struct ParamRef {
    ParamKind kind = ParamKind::CC;
    int number = 0;
    bool operator==(const ParamRef&) const = default;
    auto operator<=>(const ParamRef&) const = default;
};

struct Parameter {
    ParamKind kind = ParamKind::CC;
    int number = 0;
    std::string name;
    int minimum = 0;
    int maximum = 127;
    std::map<int, std::string> valueNames;  // optional names of values, e.g. 0 = "off"

    ParamRef ref() const { return {kind, number}; }
};

// A program (patch) in a bank, as selected by bank select MSB/LSB and program change.
struct Preset {
    int bankMsb = -1;  // -1 when the file gives no bank select value
    int bankLsb = -1;
    std::string bankName;
    int program = 0;
    std::string name;
};

// How an instrument selects banks; the values are Cakewalk's BankSelMethod numbers.
enum class BankSelect { CC0_CC32 = 0, CC0 = 1, CC32 = 2, ProgramOnly = 3 };

// A preset in bank number 'bank' (-1 for presets in no bank), with its bank select MSB/LSB filled in.
Preset makePreset(BankSelect method, int bank, const std::string& bankName, int program, const std::string& name);

struct Instrument {
    std::string name;
    std::string source;  // the file it came from, without its extension
    std::vector<Parameter> parameters;  // CCs, then RPNs, then NRPNs, each in number order
    std::vector<Preset> presets;

    // A virtual instrument (Asp .toml with based_on) copies another instrument and names the parameters to put
    // on the encoders: encoder index (from 0) = parameter name.
    std::string basedOn;
    std::map<int, std::string> macros;

    bool isVirtual() const { return !basedOn.empty(); }
    const Parameter* find(ParamRef ref) const;
    // The parameter with this name, ignoring case and surrounding spaces, or nullptr
    const Parameter* findByName(const std::string& name) const;
};

// Asp's rules (asp/instruments/live_controls.py) for names that mean an on/off switch, or that a number has no
// parameter ("*** undefined", "Reserved", ...).
bool isToggleName(const std::string& name);
bool isUnassignedName(const std::string& name);
// Whether a parameter is best on a button: a switch by name, or a range of two values.
bool isToggle(const Parameter& parameter);

// A copy of base with the virtual instrument's name and macros. Macros that name no parameter of base are left
// out and returned in unknownMacros.
Instrument resolveVirtual(const Instrument& virtualInstrument, const Instrument& base,
                          std::vector<std::string>* unknownMacros = nullptr);

// Reads a text file as UTF-8: a file that isn't valid UTF-8 is read as Windows-1252, as older .ins files are.
// A UTF-8 byte order mark is dropped. Throws std::runtime_error if it can't be read.
std::string readTextFile(const std::string& utf8Path);

// Lower case ASCII copy of s
std::string lower(std::string s);
std::string trim(const std::string& s);

}  // namespace kmidi
