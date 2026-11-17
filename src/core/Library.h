// Opening instrument files of any supported kind, and finding the instrument that a virtual (Asp .toml with
// based_on) instrument copies.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Instrument.h"

namespace kmidi {

// The file dialog wildcard and the extensions it covers
extern const char* const kInstrumentWildcard;
bool isInstrumentFile(const std::string& utf8Path);

// Every instrument in a .ins, .toml or .reabank file. A virtual .toml instrument comes back unresolved.
// Throws std::runtime_error if the file can't be read.
std::vector<Instrument> loadInstrumentFile(const std::string& utf8Path);

// Where to look for the instrument a virtual instrument is based on: the file's own directory and, for a file in
// an Asp checkout, the checkout's data/instruments and config/instruments directories.
std::vector<std::string> librarySearchDirs(const std::string& utf8Path);

// Finds the instrument with this name (ignoring case) in the .toml and .ins files under dirs, preferring a .toml
// instrument as Asp does, and resolves it if it is itself virtual. Returns nothing if there is none.
std::optional<Instrument> findInstrument(const std::vector<std::string>& dirs, const std::string& name);

// Resolves a virtual instrument by finding its base under dirs. Returns nothing if the base isn't found.
std::optional<Instrument> resolveFromLibrary(const Instrument& virtualInstrument, const std::vector<std::string>& dirs,
                                             std::vector<std::string>* unknownMacros);

}  // namespace kmidi
