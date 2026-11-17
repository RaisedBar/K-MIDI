// Self-contained tests for KMidiCore.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "History.h"
#include "InsLoader.h"
#include "Kmt.h"
#include "Layout.h"
#include "Library.h"
#include "Reabank.h"
#include "TomlLoader.h"

using namespace kmidi;
using nlohmann::json;

static int g_failures = 0, g_checks = 0;
#define CHECK(cond)                                                       \
    do {                                                                  \
        ++g_checks;                                                       \
        if (!(cond)) {                                                    \
            ++g_failures;                                                 \
            std::printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                 \
    } while (0)

static const std::string kData = KMIDI_TEST_DATA;
static const std::string kSample = std::string(KMIDI_SAMPLES) + "/kmt/Korg OPSIX Module-SE.kmt";

static std::string readBytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

static void testIns() {
    InsLoader loader;
    loader.addFile(kData + "/a-synth.ins");
    loader.addFile(kData + "/b-other.ins");
    CHECK((loader.instrumentNames(0) == std::vector<std::string>{"General MIDI", "Synth", "Looper"}));

    Instrument synth = loader.instrument(0, "Synth");
    CHECK(synth.source == "a-synth");
    // Controllers in number order, then RPNs and NRPNs
    CHECK(synth.parameters.size() == 6);
    CHECK(synth.parameters[0].number == 64 && synth.parameters[0].name == "Sustain");
    CHECK(synth.parameters[1].number == 71 && synth.parameters[1].name == "Resonance");
    CHECK(synth.parameters[3].number == 80 && synth.parameters[3].name == "Chorus On/Off");
    CHECK(synth.parameters[4].kind == ParamKind::RPN && synth.parameters[4].number == 0);
    CHECK(synth.parameters[5].kind == ParamKind::NRPN && synth.parameters[5].number == 1000);

    // Presets: bank 0 (factory), 128 (user, BasedOn factory), then the numbered list for any bank.
    // BankSelMethod=1 is CC 0 only.
    CHECK(synth.presets.size() == 2 + 3 + 128);
    CHECK(synth.presets[0].name == "Fat Bass" && synth.presets[0].bankMsb == 0 && synth.presets[0].bankLsb == -1);
    CHECK(synth.presets[3].name == "My Lead" && synth.presets[3].program == 1);
    CHECK(synth.presets[5].name == "Program 1" && synth.presets[5].bankMsb == -1);

    // Lists BasedOn each other don't loop forever
    Instrument looper = loader.instrument(0, "Looper");
    CHECK(looper.presets.size() == 2);

    // A list from another file
    Instrument borrower = loader.instrument(1, "Borrower");
    CHECK(borrower.presets.size() == 2 && borrower.presets[0].name == "Fat Bass");
    CHECK(borrower.presets[0].bankMsb == -1 && borrower.presets[0].bankLsb == -1);

    // General MIDI has no Patch entries, so uses the patch list of its own name
    CHECK(loader.instrument(0, "General MIDI").presets.size() == 3);
}

static void testToml() {
    Instrument poly = loadTomlInstrument(kData + "/lib/Test Poly.toml");
    CHECK(poly.name == "Test Poly");
    CHECK(poly.parameters.size() == 9);
    const Parameter* resonance = poly.find({ParamKind::CC, 71});
    CHECK(resonance && resonance->name == "Resonance" && resonance->maximum == 100);
    const Parameter* shape = poly.find({ParamKind::CC, 81});
    CHECK(shape && shape->valueNames.size() == 2 && shape->valueNames.at(1) == "saw");
    const Parameter* sync = poly.find({ParamKind::CC, 82});
    CHECK(sync && isToggle(*sync) && sync->valueNames.at(1) == "synced");
    const Parameter* envelope = poly.find({ParamKind::NRPN, 1000});
    CHECK(envelope && envelope->maximum == 164 && !canAssign(*envelope));
    CHECK(poly.presets.size() == 3 && poly.presets[0].bankMsb == 1 && poly.presets[2].name == "Init");

    Instrument virt = loadTomlInstrument(kData + "/virtual.toml");
    CHECK(virt.isVirtual() && virt.basedOn == "test poly" && virt.macros.size() == 4);

    std::vector<std::string> unknown;
    auto resolved = resolveFromLibrary(virt, librarySearchDirs(kData + "/virtual.toml"), &unknown);
    CHECK(resolved.has_value());
    if (resolved) {
        CHECK(resolved->name == "Test Poly (Macros)" && resolved->parameters.size() == 9);
        CHECK(resolved->macros.size() == 3);
        CHECK((unknown == std::vector<std::string>{"No Such Parameter"}));
        Layout layout = autoLayout(*resolved);
        CHECK(layout.pages.size() == 2);
        CHECK(layout.at({0, ControlKind::Knob, 0}).label == "Cutoff");
        CHECK(layout.at({0, ControlKind::Knob, 1}).label == "Resonance");
        CHECK(layout.at({0, ControlKind::Knob, 2}).empty());
        CHECK(layout.at({1, ControlKind::Knob, 1}).label == "Volume");
    }
}

static void testReabank() {
    Instrument bank = loadReabank(kData + "/Test.reabank");
    CHECK(bank.name == "Test" && bank.parameters.empty());
    CHECK(bank.presets.size() == 4);
    CHECK(bank.presets[0].name == "Before Any Bank" && bank.presets[0].bankMsb == -1);
    CHECK(bank.presets[1].bankMsb == 0 && bank.presets[1].bankLsb == 1 && bank.presets[1].bankName == "Factory B");
    CHECK(bank.presets[2].name == "Big   Pad" && bank.presets[2].program == 1);
    CHECK(bank.presets[3].bankMsb == -1 && bank.presets[3].program == 127);
    CHECK(loadInstrumentFile(kData + "/Test.reabank").size() == 1);
}

static void testLayout() {
    Instrument poly = loadTomlInstrument(kData + "/lib/Test Poly.toml");
    Layout layout = autoLayout(poly);
    // Bank select and "Reserved" are left out; switches go on buttons
    CHECK(!layout.find({ParamKind::CC, 0}) && !layout.find({ParamKind::CC, 90}));
    CHECK(layout.at({0, ControlKind::Knob, 0}).label == "Volume");
    CHECK(layout.at({0, ControlKind::Knob, 1}).label == "Resonance" && layout.at({0, ControlKind::Knob, 1}).maximum == 100);
    CHECK(layout.at({0, ControlKind::Button, 0}).label == "Chorus On/Off");
    CHECK(layout.at({0, ControlKind::Button, 1}).label == "Sync" && layout.at({0, ControlKind::Button, 1}).maximum == 1);
    CHECK(layout.at({0, ControlKind::Knob, 3}).label == "LFO Shape");
    CHECK(!layout.find({ParamKind::NRPN, 1000}));

    // Moving down swaps with the next knob, and past knob 8 onto a new page
    SlotId volume{0, ControlKind::Knob, 0};
    auto moved = layout.move(volume, 1);
    CHECK(moved && *moved == (SlotId{0, ControlKind::Knob, 1}));
    CHECK(layout.at({0, ControlKind::Knob, 0}).label == "Resonance");
    CHECK(!layout.move(SlotId{0, ControlKind::Knob, 0}, -1));
    auto last = layout.move(SlotId{0, ControlKind::Knob, 7}, 1);
    CHECK(last && last->page == 1 && layout.pages.size() == 2);
    layout.removeEmptyTrailingPages();
    CHECK(layout.pages.size() == 1);
    auto nextPage = layout.moveToPage(SlotId{0, ControlKind::Knob, 1}, 1);
    CHECK(nextPage && *nextPage == (SlotId{1, ControlKind::Knob, 1}) && layout.at(*nextPage).label == "Volume");

    // Switching a knob to a button keeps its label; assigning again moves rather than copies
    auto button = layout.switchKind(*nextPage, poly);
    CHECK(button && button->kind == ControlKind::Button && button->page == 1 && layout.at(*button).label == "Volume");
    SlotId placed = layout.assignNext(*poly.find({ParamKind::CC, 7}), ControlKind::Knob, SlotId{0, ControlKind::Knob, 0});
    CHECK(placed.page == 0 && layout.at(placed).label == "Volume" && layout.at(*button).empty());
}


static void testHistory() {
    Instrument poly = loadTomlInstrument(kData + "/lib/Test Poly.toml");
    Layout layout = autoLayout(poly);
    History history;
    CHECK(history.atSaved() && !history.canUndo() && !history.canRedo());

    // A move, then undo and redo
    Layout before = layout;
    SlotId volume{0, ControlKind::Knob, 0};
    auto moved = layout.move(volume, 1);
    history.record("Move Volume right", before, volume);
    CHECK(history.canUndo() && history.undoDescription() == "Move Volume right" && !history.atSaved());
    HistoryEntry back = history.undo(layout, moved);
    layout = back.layout;
    CHECK(layout.at(volume).label == "Volume" && back.selection == volume);
    CHECK(history.atSaved() && history.canRedo() && history.redoDescription() == "Move Volume right");
    HistoryEntry forward = history.redo(layout, back.selection);
    layout = forward.layout;
    CHECK(layout.at(*moved).label == "Volume" && forward.selection == moved && !history.atSaved());

    // Typing in the name field merges into one change, until the template is saved
    history.markSaved();
    for (const char* name : {"A", "AB", "ABC"}) {
        Layout b = layout;
        layout.name = name;
        history.record("Change template name", b, std::nullopt, true);
    }
    Layout b = layout;
    layout.name = "ABCD";
    history.record("Change template name", b, std::nullopt, true);
    layout = history.undo(layout, std::nullopt).layout;
    CHECK(layout.name == "Test Poly" && history.atSaved());
    CHECK(history.undoDescription() == "Move Volume right");

    // After an undo, a new change starts a new entry rather than joining an older one
    layout = history.redo(layout, std::nullopt).layout;
    CHECK(layout.name == "ABCD");
    layout = history.undo(layout, std::nullopt).layout;
    b = layout;
    layout.name = "X";
    history.record("Change template name", b, std::nullopt, true);
    CHECK(!history.canRedo() && history.undoDescription() == "Change template name");
    layout = history.undo(layout, std::nullopt).layout;
    CHECK(layout.name == "Test Poly" && history.undoDescription() == "Move Volume right");

    // A change made after undoing past the saved state means it can't be reached again
    history.clear();
    b = layout;
    layout.name = "Saved";
    history.record("Change template name", b, std::nullopt);
    history.markSaved();
    layout = history.undo(layout, std::nullopt).layout;
    CHECK(!history.atSaved());
    b = layout;
    layout.colour = "Red";
    history.record("Change colour", b, std::nullopt);
    CHECK(!history.atSaved());
    layout = history.undo(layout, std::nullopt).layout;
    CHECK(!history.atSaved());
}

static void testKmt() {
    // The sample made by Komplete Kontrol reads, and writes back with the same structure
    Layout sample = loadKmt(kSample);
    CHECK(sample.name == "Korg OPSIX Module/SE" && sample.pages.size() == 2);
    CHECK(sample.at({0, ControlKind::Knob, 0}).label == "Volume" && sample.at({0, ControlKind::Knob, 0}).maximum == 100);
    CHECK(sample.at({0, ControlKind::Knob, 1}).minimum == 1);
    CHECK(sample.at({0, ControlKind::Button, 2}).mode == ButtonMode::Increment &&
          sample.at({0, ControlKind::Button, 2}).maximum == 39);
    CHECK(sample.at({0, ControlKind::Button, 0}).colour == "Red");
    CHECK(sample.pages[1].empty());  // its second page has only Komplete Kontrol's own "CC n" controls

    std::string original = readBytes(kSample);
    Instrument none;
    std::string written = writeKmt(sample, none);
    json a = json::parse(original), b = json::parse(written);
    CHECK(a["pages"][0] == b["pages"][0]);
    CHECK(a["image_data"] == b["image_data"] && a["pedal_assignments"] == b["pedal_assignments"]);
    CHECK(written.find("\r\n") != std::string::npos && written.find("\n") == written.find("\r\n") + 1);
    CHECK(written.substr(0, 20) == original.substr(0, 20));  // same indenting and key order

    // A new template from an instrument has the same keys as the sample, at every level
    Instrument poly = loadTomlInstrument(kData + "/lib/Test Poly.toml");
    Layout layout = autoLayout(poly);
    layout.midiChannel = 2;
    layout.assign({0, ControlKind::Knob, 7}, *poly.find({ParamKind::NRPN, 1000}));  // can't be sent
    std::vector<std::string> warnings;
    json fresh = json::parse(writeKmt(layout, poly, &warnings));
    CHECK(warnings.size() == 1);
    std::function<bool(const json&, const json&)> sameKeys = [&](const json& x, const json& y) {
        if (x.is_object() != y.is_object()) return false;
        if (!x.is_object()) return true;
        for (auto& [key, value] : x.items()) {
            if (key == "image_data" || key == "hide_template_name") continue;  // optional in a template
            if (!y.contains(key)) {
                std::printf("missing key %s\n", key.c_str());
                return false;
            }
            if (!y[key].is_null() && !value.is_null() && value.is_object() && !sameKeys(value, y[key])) return false;
        }
        return true;
    };
    CHECK(sameKeys(a, fresh));
    CHECK(sameKeys(a["pages"][0]["knobs"][0], fresh["pages"][0]["knobs"][0]));
    CHECK(sameKeys(a["pages"][0]["buttons"][0], fresh["pages"][0]["buttons"][0]));
    CHECK(fresh["pages"][0]["knobs"][0]["mapping"]["assignment"]["cc"]["midi_channel"] == 2);
    CHECK(fresh["keyboard_assignments"][0]["midi_channel"] == 2);
    CHECK(fresh["pages"][0]["buttons"][0]["mapping"]["behaviour"]["toggle"]["on"] == 127);
    CHECK(fresh["pages"][0]["knobs"][3]["label"] == "LFO Shape");
    // Empty controls get CCs the instrument doesn't use
    const json& blank = fresh["pages"][0]["knobs"][7];
    int cc = blank["mapping"]["assignment"]["cc"]["midi_cc_number"];
    CHECK(blank["custom_label"] == false && blank["label"] == "CC " + std::to_string(cc));
    CHECK(!poly.find({ParamKind::CC, cc}));

    // And reads back as the same layout
    Layout back = parseKmt(fresh.dump());
    CHECK(back.at({0, ControlKind::Knob, 0}).label == "Volume" && back.at({0, ControlKind::Knob, 7}).empty());
    CHECK(back.midiChannel == 2);
}

// KMidiTests --library <dir>: reads every instrument file under dir, lays each instrument out and writes its
// template, reporting files that fail. A check against a real instrument library rather than the fixtures.
static int checkLibrary(const std::string& dir) {
    int files = 0, failed = 0, instruments = 0, virtuals = 0;
    for (auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::path(std::u8string(dir.begin(), dir.end())))) {
        auto u8 = entry.path().u8string();
        std::string path(u8.begin(), u8.end());
        if (!entry.is_regular_file() || !isInstrumentFile(path)) continue;
        ++files;
        try {
            for (Instrument instrument : loadInstrumentFile(path)) {
                ++instruments;
                if (instrument.isVirtual()) {
                    ++virtuals;
                    std::vector<std::string> unknown;
                    auto resolved = resolveFromLibrary(instrument, librarySearchDirs(path), &unknown);
                    if (!resolved) throw std::runtime_error("base instrument " + instrument.basedOn + " not found");
                    for (const std::string& u : unknown) std::printf("  %s: unknown macro %s\n", path.c_str(), u.c_str());
                    instrument = *resolved;
                }
                (void)json::parse(writeKmt(autoLayout(instrument), instrument));
            }
        } catch (const std::exception& e) {
            ++failed;
            std::printf("FAILED %s: %s\n", path.c_str(), e.what());
        }
    }
    std::printf("%d files, %d instruments (%d virtual), %d files failed\n", files, instruments, virtuals, failed);
    return failed ? 1 : 0;
}

int main(int argc, char** argv) {
    if (argc == 3 && std::string(argv[1]) == "--library") return checkLibrary(argv[2]);
    struct Test {
        const char* name;
        void (*run)();
    };
    for (const Test& test : {Test{"ins", testIns}, Test{"toml", testToml}, Test{"reabank", testReabank},
                             Test{"layout", testLayout}, Test{"history", testHistory}, Test{"kmt", testKmt}}) {
        try {
            test.run();
        } catch (const std::exception& e) {
            ++g_failures;
            std::printf("FAILED %s: exception: %s\n", test.name, e.what());
        }
    }
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
