// Reads an instrument from Asp's .toml format (asp/instruments/formats/toml_loader.py). Only what a MIDI template
// needs is read: the name, the [controllers], [rpns] and [nrpns] tables (names, ranges and value names), the
// presets, and a virtual instrument's based_on and [macros].
#pragma once

#include <string>

#include "Instrument.h"

namespace kmidi {

// Throws std::runtime_error for a file that can't be read or isn't a valid instrument. A virtual instrument comes
// back unresolved: with basedOn and macros, and no parameters (see resolveVirtual and Library).
Instrument loadTomlInstrument(const std::string& utf8Path);

// Just the [instrument] name of a .toml file, or "" if it has none or can't be read.
std::string tomlInstrumentName(const std::string& utf8Path);

}  // namespace kmidi
