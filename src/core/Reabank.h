// Reads REAPER .reabank files, which name the presets of a device:
//
//     // comment
//     Bank 0 1 Factory B        <- "Bank <MSB> <LSB> <name>", for the programs that follow
//     0 Init Patch              <- "<program> <name>"
//
// They have no controller names, so the instrument has presets and no parameters.
#pragma once

#include <string>

#include "Instrument.h"

namespace kmidi {

// The instrument is named after the file. Throws std::runtime_error if the file can't be read.
Instrument loadReabank(const std::string& utf8Path);

}  // namespace kmidi
