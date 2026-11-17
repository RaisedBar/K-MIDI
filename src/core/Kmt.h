// Native Instruments Kontrol S MK3 MIDI template (.kmt) files: UTF-8 JSON with CRLF line endings and 4-space
// indents, keys in alphabetical order. See samples/kmt for a template made by Komplete Kontrol.
#pragma once

#include <string>
#include <vector>

#include "Instrument.h"
#include "Layout.h"

namespace kmidi {

// The .kmt text for a layout. Empty slots are given CCs that the instrument doesn't use, labelled "CC n" as
// Komplete Kontrol labels unnamed controls, so touching one can't change a parameter. Slots that can't be sent
// (an RPN or NRPN) are left empty and described in warnings.
std::string writeKmt(const Layout& layout, const Instrument& instrument, std::vector<std::string>* warnings = nullptr);
// Writes the file. Throws std::runtime_error if it can't be written.
void saveKmt(const std::string& utf8Path, const Layout& layout, const Instrument& instrument,
             std::vector<std::string>* warnings = nullptr);

// Reads a .kmt file into a layout. Controls with Komplete Kontrol's own "CC n" labels are read as empty slots.
// Throws std::runtime_error if the file can't be read or isn't a MIDI template.
Layout loadKmt(const std::string& utf8Path);
Layout parseKmt(const std::string& text);

}  // namespace kmidi
