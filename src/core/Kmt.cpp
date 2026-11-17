#include "Kmt.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>

#include <nlohmann/json.hpp>

using nlohmann::json;

namespace kmidi {

namespace {

json ccAssignment(int cc, int channel) {
    return {{"cc", {{"midi_cc_number", cc}, {"midi_channel", channel}}}};
}

// The parts of a template that this tool doesn't edit, as Komplete Kontrol sets them in a new template
json defaultTemplate() {
    return json::parse(R"({
        "color": "Violet",
        "hide_template_name": false,
        "keyboard_assignments": [
            {"color": "Violet", "disable": false, "midi_channel": 0, "priority": "plugin",
             "semitones_transpose": 0, "upper_key": 127, "velocity_curve": null}
        ],
        "pages": [],
        "pedal_assignments": {
            "PedalAssign1": null,
            "PedalAssign2": null,
            "PedalExpression": {"pedal_continuous_configuration": {
                "configuration": {"invert": false, "swap_tip_ring": false},
                "mapping": {"assignment": {"cc": {"midi_cc_number": 11, "midi_channel": 0}},
                            "behaviour": {"range": {"max": 127, "min": 0}}}}},
            "PedalSustain": {"pedal_switch_configuration": {
                "configuration": {"invert": false},
                "mapping_ring": {"assignment": {"cc": {"midi_cc_number": 65, "midi_channel": 0}},
                                 "behaviour": {"gate": {"off": 0, "on": 127}}},
                "mapping_tip": {"assignment": {"cc": {"midi_cc_number": 64, "midi_channel": 0}},
                                "behaviour": {"gate": {"off": 0, "on": 127}}}}}
        },
        "template_name": "",
        "touchstrip_assignments": {
            "TouchStrip": {"assignment": {"cc": {"midi_cc_number": 5, "midi_channel": 0}},
                           "behaviour": {"range": {"max": 127, "min": 0}}}
        },
        "wheel_assignments": {
            "Modulation": {"assignment": {"cc": {"midi_cc_number": 1, "midi_channel": 0}},
                           "behaviour": {"range": {"max": 127, "min": 0}}},
            "Pitchbend": {"assignment": {"pitch_bend": {"midi_channel": 0}},
                          "behaviour": {"range": {"max": 16383, "min": 0}}}
        }
    })");
}

// Sets every "midi_channel" in the template to the channel, so the keys, wheels and pedals follow it
void setChannels(json& node, int channel) {
    if (node.is_object()) {
        for (auto& [key, value] : node.items()) {
            if (key == "midi_channel" && value.is_number_integer())
                value = channel;
            else
                setChannels(value, channel);
        }
    } else if (node.is_array()) {
        for (json& value : node) setChannels(value, channel);
    }
}

// Hands out CCs for empty slots: MIDI's undefined CCs that the instrument and layout don't use
class SpareCcs {
public:
    SpareCcs(const Layout& layout, const Instrument& instrument) {
        std::set<int> used;
        for (const Parameter& p : instrument.parameters)
            if (p.kind == ParamKind::CC) used.insert(p.number);
        for (const Page& page : layout.pages)
            for (int i = 0; i < kControlsPerRow; ++i)
                for (const Slot* slot : {&page.knobs[i], &page.buttons[i]})
                    if (slot->param && slot->param->kind == ParamKind::CC) used.insert(slot->param->number);
        std::vector<int> candidates;
        for (int cc = 14; cc <= 31; ++cc) candidates.push_back(cc);
        for (int cc = 102; cc <= 119; ++cc) candidates.push_back(cc);
        for (int cc = 85; cc <= 90; ++cc) candidates.push_back(cc);
        candidates.push_back(3);
        candidates.push_back(9);
        for (int cc : candidates)
            if (!used.count(cc)) spare_.push_back(cc);
        if (spare_.empty()) spare_ = candidates;  // nothing is unused, so share them
    }
    int next() { return spare_[next_++ % spare_.size()]; }

private:
    std::vector<int> spare_;
    size_t next_ = 0;
};

json knobJson(const Slot& slot, int cc, int channel, bool custom) {
    return {{"custom_label", custom},
            {"display_mode", slot.display == KnobDisplay::Percent ? "unipolar_percent" : "unipolar_int"},
            {"label", slot.label},
            {"mapping",
             {{"assignment", ccAssignment(cc, channel)},
              {"behaviour", {{"absolute", {{"max", slot.maximum}, {"min", slot.minimum}}}}}}}};
}

json buttonJson(const Slot& slot, int cc, int channel, bool custom) {
    json behaviour;
    if (slot.mode == ButtonMode::Increment)
        behaviour["increment"] = {{"max", slot.maximum}, {"min", slot.minimum}, {"step", slot.step}, {"wrap", slot.wrap}};
    else
        behaviour["toggle"] = {{"off", slot.minimum}, {"on", slot.maximum}};
    return {{"color", slot.colour},
            {"custom_label", custom},
            {"label", slot.label},
            {"mapping", {{"assignment", ccAssignment(cc, channel)}, {"behaviour", behaviour}}}};
}

std::filesystem::path toPath(const std::string& utf8) {
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

// Reads one knob or button into a slot
Slot readSlot(const json& control, ControlKind kind) {
    Slot slot;
    if (!control.is_object()) return slot;
    const json* mapping = control.contains("mapping") ? &control["mapping"] : nullptr;
    if (!mapping || !mapping->is_object()) return slot;
    const json& assignment = mapping->value("assignment", json::object());
    const json& behaviour = mapping->value("behaviour", json::object());

    std::string label = control.value("label", std::string());
    bool custom = control.value("custom_label", true);
    if (!custom || !assignment.contains("cc")) return slot;  // Komplete Kontrol's own "CC n" control, or not a CC

    slot.param = ParamRef{ParamKind::CC, assignment["cc"].value("midi_cc_number", 0)};
    slot.label = label;
    if (kind == ControlKind::Knob) {
        slot.display = control.value("display_mode", std::string()) == "unipolar_percent" ? KnobDisplay::Percent
                                                                                         : KnobDisplay::Integer;
        const json& absolute = behaviour.value("absolute", json::object());
        slot.minimum = absolute.value("min", 0);
        slot.maximum = absolute.value("max", 127);
    } else {
        slot.colour = control.value("color", std::string("Mint"));
        if (behaviour.contains("increment")) {
            const json& increment = behaviour["increment"];
            slot.mode = ButtonMode::Increment;
            slot.minimum = increment.value("min", 0);
            slot.maximum = increment.value("max", 127);
            slot.step = increment.value("step", 1);
            slot.wrap = increment.value("wrap", true);
        } else {
            const json& toggle = behaviour.value("toggle", json::object());
            slot.mode = ButtonMode::Toggle;
            slot.minimum = toggle.value("off", 0);
            slot.maximum = toggle.value("on", 127);
        }
    }
    return slot;
}

}  // namespace

std::string writeKmt(const Layout& layout, const Instrument& instrument, std::vector<std::string>* warnings) {
    json doc = layout.kmtBase.empty() ? defaultTemplate() : json::parse(layout.kmtBase);
    int channel = std::clamp(layout.midiChannel, 0, 15);
    setChannels(doc, channel);
    doc["template_name"] = layout.name;
    doc["color"] = layout.colour;

    SpareCcs spare(layout, instrument);
    json pages = json::array();
    for (int p = 0; p < static_cast<int>(layout.pages.size()); ++p) {
        const Page& page = layout.pages[p];
        json knobs = json::array(), buttons = json::array();
        for (int i = 0; i < kControlsPerRow; ++i) {
            for (ControlKind kind : {ControlKind::Knob, ControlKind::Button}) {
                Slot slot = kind == ControlKind::Knob ? page.knobs[i] : page.buttons[i];
                bool custom = true;
                int cc = 0;
                if (slot.param && slot.param->kind != ParamKind::CC) {
                    if (warnings)
                        warnings->push_back(describe({p, kind, i}) + " (" + slot.label + ") is an " +
                                            kindName(slot.param->kind) +
                                            ", which a Kontrol S MK3 template can't send, so it was left empty.");
                    slot.param.reset();
                }
                if (slot.param) {
                    cc = slot.param->number;
                } else {
                    Slot blank;  // Komplete Kontrol's defaults for an unnamed control
                    blank.colour = slot.colour;
                    slot = blank;
                    cc = spare.next();
                    slot.label = "CC " + std::to_string(cc);
                    custom = false;
                }
                if (kind == ControlKind::Knob)
                    knobs.push_back(knobJson(slot, cc, channel, custom));
                else
                    buttons.push_back(buttonJson(slot, cc, channel, custom));
            }
        }
        pages.push_back({{"buttons", buttons}, {"knobs", knobs}});
    }
    doc["pages"] = pages;

    // Komplete Kontrol writes CRLF line endings and no newline at the end
    std::string text = doc.dump(4);
    std::string out;
    out.reserve(text.size() + text.size() / 16);
    for (char c : text) {
        if (c == '\n') out += '\r';
        out += c;
    }
    return out;
}

void saveKmt(const std::string& utf8Path, const Layout& layout, const Instrument& instrument,
             std::vector<std::string>* warnings) {
    std::string text = writeKmt(layout, instrument, warnings);
    std::ofstream out(toPath(utf8Path), std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Can't write " + utf8Path);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!out) throw std::runtime_error("Couldn't write all of " + utf8Path);
}

Layout parseKmt(const std::string& text) {
    json doc;
    try {
        doc = json::parse(text);
    } catch (const json::exception& e) {
        throw std::runtime_error(std::string("Not a valid .kmt file: ") + e.what());
    }
    if (!doc.is_object() || !doc.contains("pages") || !doc["pages"].is_array())
        throw std::runtime_error("Not a MIDI template: it has no pages");

    Layout layout;
    layout.name = doc.value("template_name", std::string());
    layout.colour = doc.value("color", std::string("Violet"));
    if (doc.contains("keyboard_assignments") && doc["keyboard_assignments"].is_array() &&
        !doc["keyboard_assignments"].empty())
        layout.midiChannel = doc["keyboard_assignments"][0].value("midi_channel", 0);

    layout.pages.clear();
    for (const json& pageJson : doc["pages"]) {
        Page page;
        for (ControlKind kind : {ControlKind::Knob, ControlKind::Button}) {
            const char* key = kind == ControlKind::Knob ? "knobs" : "buttons";
            if (!pageJson.contains(key) || !pageJson[key].is_array()) continue;
            const json& controls = pageJson[key];
            for (int i = 0; i < kControlsPerRow && i < static_cast<int>(controls.size()); ++i)
                (kind == ControlKind::Knob ? page.knobs : page.buttons)[i] = readSlot(controls[i], kind);
        }
        layout.pages.push_back(page);
    }
    if (layout.pages.empty()) layout.pages.emplace_back();

    json base = doc;
    base["pages"] = json::array();
    layout.kmtBase = base.dump();
    return layout;
}

Layout loadKmt(const std::string& utf8Path) {
    std::ifstream in(toPath(utf8Path), std::ios::binary);
    if (!in) throw std::runtime_error("Can't open " + utf8Path);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (text.rfind("\xEF\xBB\xBF", 0) == 0) text.erase(0, 3);
    return parseKmt(text);
}

}  // namespace kmidi
