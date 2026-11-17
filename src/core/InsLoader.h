// Reads Cakewalk instrument definition (.ins) files, as Asp does (asp/instruments/formats/cakewalk.py).
//
// An instrument can use name lists (patch, controller, RPN and NRPN names) from other files, so every file is
// added first, then each instrument is built from all of them, preferring its own file's lists.
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "Instrument.h"

namespace kmidi {

class InsLoader {
public:
    // Throws std::runtime_error if the file can't be read.
    void addFile(const std::string& utf8Path);

    // The names of the instruments defined in an added file, in file order.
    std::vector<std::string> instrumentNames(size_t fileIndex) const;
    // Builds an instrument defined in an added file. Throws std::runtime_error if the file doesn't define it.
    Instrument instrument(size_t fileIndex, const std::string& name) const;
    // Every instrument from every added file, in file order.
    std::vector<Instrument> allInstruments() const;

    size_t fileCount() const { return files_.size(); }

private:
    using Entries = std::vector<std::pair<std::string, std::string>>;  // key = value, in file order
    using Lists = std::vector<std::pair<std::string, Entries>>;         // [list name] -> entries
    struct File {
        std::string source;
        std::vector<std::pair<std::string, Lists>> sections;  // ".Section Type" -> lists
        const Lists* section(const std::string& type) const;
    };

    const Entries* findList(const std::string& sectionType, const std::string& name, size_t fileIndex) const;
    std::vector<std::pair<int, std::string>> resolveList(const std::string& sectionType, const std::string& name,
                                                          size_t fileIndex, int depth = 0) const;
    Instrument build(const std::string& name, const Entries& entries, size_t fileIndex) const;

    std::vector<File> files_;
};

// Every instrument in one .ins file. Throws std::runtime_error if it can't be read.
std::vector<Instrument> loadInsFile(const std::string& utf8Path);

}  // namespace kmidi
