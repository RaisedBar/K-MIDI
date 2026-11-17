#include "Layout.h"

#include <algorithm>
#include <utility>

namespace kmidi {

const std::vector<std::string> kConfirmedColours = {"Violet", "Fuchsia", "Red", "Mint"};
const std::vector<std::string> kColours = {"Violet", "Fuchsia", "Red", "Mint", "Orange", "Yellow", "Lime",
                                           "Green",  "Cyan",    "Turquoise", "Blue", "Plum", "Purple", "Magenta",
                                           "White"};

bool Page::empty() const {
    for (int i = 0; i < kControlsPerRow; ++i)
        if (!knobs[i].empty() || !buttons[i].empty()) return false;
    return true;
}

std::string describe(SlotId id) {
    return "Page " + std::to_string(id.page + 1) + (id.kind == ControlKind::Knob ? ", knob " : ", button ") +
           std::to_string(id.index + 1);
}

bool canAssign(const Parameter& parameter) { return parameter.kind == ParamKind::CC; }

Slot makeSlot(const Parameter& parameter, ControlKind kind) {
    Slot slot;
    slot.param = parameter.ref();
    slot.label = trim(parameter.name);
    slot.minimum = std::clamp(parameter.minimum, 0, 127);
    slot.maximum = std::clamp(parameter.maximum, slot.minimum, 127);
    if (kind == ControlKind::Button) {
        // A switch toggles between its two values; anything else with a few values steps through them
        int count = slot.maximum - slot.minimum + 1;
        slot.mode = isToggle(parameter) || count <= 2 || count > 32 ? ButtonMode::Toggle : ButtonMode::Increment;
    }
    return slot;
}

Slot& Layout::at(SlotId id) {
    ensurePages(id.page + 1);
    Page& page = pages[id.page];
    return id.kind == ControlKind::Knob ? page.knobs[id.index] : page.buttons[id.index];
}

const Slot& Layout::at(SlotId id) const {
    const Page& page = pages.at(id.page);
    return id.kind == ControlKind::Knob ? page.knobs[id.index] : page.buttons[id.index];
}

std::optional<SlotId> Layout::find(ParamRef ref) const {
    for (int p = 0; p < static_cast<int>(pages.size()); ++p)
        for (ControlKind kind : {ControlKind::Knob, ControlKind::Button})
            for (int i = 0; i < kControlsPerRow; ++i)
                if (at({p, kind, i}).param == ref) return SlotId{p, kind, i};
    return std::nullopt;
}

void Layout::ensurePages(int count) {
    while (static_cast<int>(pages.size()) < count) pages.emplace_back();
}

void Layout::assign(SlotId id, const Parameter& parameter) {
    if (auto existing = find(parameter.ref())) clear(*existing);
    at(id) = makeSlot(parameter, id.kind);
}

SlotId Layout::assignNext(const Parameter& parameter, ControlKind kind, SlotId from) {
    if (auto existing = find(parameter.ref())) clear(*existing);
    int n = from.kind == kind ? linear(from) : from.page * kControlsPerRow;
    while (!at(fromLinear(n, kind)).empty()) ++n;  // at() adds pages as needed
    SlotId id = fromLinear(n, kind);
    at(id) = makeSlot(parameter, kind);
    return id;
}

void Layout::clear(SlotId id) { at(id) = Slot{}; }

std::optional<SlotId> Layout::move(SlotId id, int delta) {
    int target = linear(id) + delta;
    if (target < 0) return std::nullopt;
    SlotId to = fromLinear(target, id.kind);
    std::swap(at(id), at(to));
    return to;
}

std::optional<SlotId> Layout::moveToPage(SlotId id, int delta) {
    int page = id.page + delta;
    if (page < 0) return std::nullopt;
    SlotId to{page, id.kind, id.index};
    std::swap(at(id), at(to));
    return to;
}

std::optional<SlotId> Layout::switchKind(SlotId id, const Instrument& instrument) {
    const Slot& slot = at(id);
    if (slot.empty()) return std::nullopt;
    ControlKind other = id.kind == ControlKind::Knob ? ControlKind::Button : ControlKind::Knob;
    std::string label = slot.label;
    const Parameter* parameter = instrument.find(*slot.param);
    Parameter fallback;
    if (!parameter) {
        fallback.kind = slot.param->kind;
        fallback.number = slot.param->number;
        fallback.name = label;
        parameter = &fallback;
    }
    Parameter copy = *parameter;
    clear(id);
    SlotId to = assignNext(copy, other, SlotId{id.page, other, id.index});
    at(to).label = label;  // keep a label the user has edited
    return to;
}

void Layout::addPage() { pages.emplace_back(); }

void Layout::removeEmptyTrailingPages() {
    while (pages.size() > 1 && pages.back().empty()) pages.pop_back();
}

Layout autoLayout(const Instrument& instrument) {
    Layout layout;
    layout.name = instrument.name;

    if (!instrument.macros.empty()) {
        for (const auto& [encoder, name] : instrument.macros) {
            const Parameter* p = instrument.findByName(name);
            if (!p || !canAssign(*p) || layout.find(p->ref())) continue;
            layout.assign({encoder / kControlsPerRow, ControlKind::Knob, encoder % kControlsPerRow}, *p);
        }
        return layout;
    }

    SlotId nextKnob{0, ControlKind::Knob, 0}, nextButton{0, ControlKind::Button, 0};
    for (const Parameter& p : instrument.parameters) {
        if (!canAssign(p) || isUnassignedName(p.name)) continue;
        // Bank select, data entry and RPN/NRPN selection aren't useful on a knob
        if (p.number == 0 || p.number == 32 || p.number == 6 || p.number == 38 || (p.number >= 98 && p.number <= 101))
            continue;
        if (isToggle(p))
            nextButton = layout.assignNext(p, ControlKind::Button, nextButton);
        else
            nextKnob = layout.assignNext(p, ControlKind::Knob, nextKnob);
    }
    return layout;
}

}  // namespace kmidi
