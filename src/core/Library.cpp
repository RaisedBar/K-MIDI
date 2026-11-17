#include "Library.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>

#include "InsLoader.h"
#include "Reabank.h"
#include "TomlLoader.h"

namespace fs = std::filesystem;

namespace kmidi {

const char* const kInstrumentWildcard =
    "Instrument definitions (*.ins;*.toml;*.reabank)|*.ins;*.toml;*.reabank|"
    "Cakewalk instrument definitions (*.ins)|*.ins|Asp instruments (*.toml)|*.toml|"
    "REAPER bank files (*.reabank)|*.reabank|All files (*.*)|*.*";

namespace {

fs::path toPath(const std::string& utf8) { return fs::path(std::u8string(utf8.begin(), utf8.end())); }

std::string toUtf8(const fs::path& p) {
    auto s = p.u8string();
    return std::string(s.begin(), s.end());
}

std::string extensionOf(const std::string& utf8Path) { return lower(toUtf8(toPath(utf8Path).extension())); }

// Files with this extension under dir, in path order
std::vector<std::string> filesUnder(const std::string& dir, const char* extension) {
    std::vector<std::string> files;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(toPath(dir), fs::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec) && lower(toUtf8(it->path().extension())) == extension)
            files.push_back(toUtf8(it->path()));
    }
    std::sort(files.begin(), files.end(), [](const std::string& a, const std::string& b) { return lower(a) < lower(b); });
    return files;
}

std::optional<Instrument> findInstrument(const std::vector<std::string>& dirs, const std::string& name, int depth) {
    if (depth > 16) return std::nullopt;  // virtual instruments based on each other
    std::string wanted = lower(trim(name));

    for (const std::string& dir : dirs)
        for (const std::string& file : filesUnder(dir, ".toml")) {
            // Cheap check before parsing: the name must appear in the file
            std::string text;
            try {
                text = readTextFile(file);
            } catch (const std::exception&) {
                continue;
            }
            if (lower(text).find(wanted) == std::string::npos) continue;
            if (lower(trim(tomlInstrumentName(file))) != wanted) continue;
            try {
                Instrument instrument = loadTomlInstrument(file);
                if (!instrument.isVirtual()) return instrument;
                auto base = findInstrument(dirs, instrument.basedOn, depth + 1);
                if (base) return resolveVirtual(instrument, *base);
            } catch (const std::exception&) {
            }
        }

    // .ins files can use each other's name lists, so they are loaded together
    InsLoader loader;
    for (const std::string& dir : dirs)
        for (const std::string& file : filesUnder(dir, ".ins")) {
            try {
                loader.addFile(file);
            } catch (const std::exception&) {
            }
        }
    for (size_t i = 0; i < loader.fileCount(); ++i)
        for (const std::string& instrumentName : loader.instrumentNames(i))
            if (lower(trim(instrumentName)) == wanted) return loader.instrument(i, instrumentName);
    return std::nullopt;
}

}  // namespace

bool isInstrumentFile(const std::string& utf8Path) {
    std::string ext = extensionOf(utf8Path);
    return ext == ".ins" || ext == ".toml" || ext == ".reabank";
}

std::vector<Instrument> loadInstrumentFile(const std::string& utf8Path) {
    std::string ext = extensionOf(utf8Path);
    if (ext == ".toml") return {loadTomlInstrument(utf8Path)};
    if (ext == ".reabank") return {loadReabank(utf8Path)};
    if (ext == ".ins") return loadInsFile(utf8Path);
    throw std::runtime_error("K-MIDI can't read " + ext + " files. It reads .ins, .toml and .reabank files.");
}

std::vector<std::string> librarySearchDirs(const std::string& utf8Path) {
    std::vector<std::string> dirs;
    auto add = [&](const fs::path& dir) {
        std::error_code ec;
        if (!fs::is_directory(dir, ec)) return;
        std::string s = toUtf8(fs::weakly_canonical(dir, ec));
        // Skip a directory inside one already listed, which would be searched twice
        for (const std::string& existing : dirs)
            if (lower(s).rfind(lower(existing), 0) == 0) return;
        dirs.push_back(s);
    };

    fs::path dir = toPath(utf8Path).parent_path();
    std::vector<fs::path> aspDirs;
    for (fs::path p = dir; !p.empty(); p = p.parent_path()) {
        std::error_code ec;
        if (fs::is_directory(p / "data" / "instruments", ec) || fs::is_directory(p / "config" / "instruments", ec)) {
            aspDirs = {p / "config" / "instruments", p / "data" / "instruments"};
            break;
        }
        if (p == p.parent_path()) break;
    }
    for (const fs::path& p : aspDirs) add(p);
    add(dir);
    return dirs;
}

std::optional<Instrument> findInstrument(const std::vector<std::string>& dirs, const std::string& name) {
    return findInstrument(dirs, name, 0);
}

std::optional<Instrument> resolveFromLibrary(const Instrument& virtualInstrument, const std::vector<std::string>& dirs,
                                             std::vector<std::string>* unknownMacros) {
    auto base = findInstrument(dirs, virtualInstrument.basedOn);
    if (!base) return std::nullopt;
    return resolveVirtual(virtualInstrument, *base, unknownMacros);
}

}  // namespace kmidi
