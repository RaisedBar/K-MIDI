// A Kontrol S MK3 MIDI template being laid out: pages of 8 knobs and 8 buttons, each of which may send one of
// the instrument's parameters.
#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "Instrument.h"

namespace kmidi {

constexpr int kControlsPerRow = 8;

enum class ControlKind { Knob, Button };
enum class KnobDisplay { Integer, Percent };     // .kmt display_mode unipolar_int / unipolar_percent
enum class ButtonMode { Toggle, Increment };     // .kmt behaviour toggle / increment

// Colour names known to be valid in .kmt files (seen in templates made by Komplete Kontrol)
extern const std::vector<std::string> kConfirmedColours;
// Every colour name offered, the confirmed ones first
extern const std::vector<std::string> kColours;

struct Slot {
    std::optional<ParamRef> param;  // nothing for an empty slot
    std::string label;              // shown on the keyboard's display
    int minimum = 0;                // knob: absolute range; toggle button: off value; increment: first value
    int maximum = 127;              // knob: absolute range; toggle button: on value; increment: last value
    KnobDisplay display = KnobDisplay::Integer;
    ButtonMode mode = ButtonMode::Toggle;
    int step = 1;                   // increment buttons
    bool wrap = true;               // increment buttons: go back to the first value after the last
    std::string colour = "Mint";    // buttons

    bool empty() const { return !param.has_value(); }
    bool operator==(const Slot&) const = default;
};

struct Page {
    std::array<Slot, kControlsPerRow> knobs;
    std::array<Slot, kControlsPerRow> buttons;
    bool empty() const;
};

struct SlotId {
    int page = 0;
    ControlKind kind = ControlKind::Knob;
    int index = 0;
    bool operator==(const SlotId&) const = default;
};

// "Page 2, knob 3"
std::string describe(SlotId id);

// A slot set up to send a parameter on a knob or button, with its name as the label and its range clamped to 0-127.
Slot makeSlot(const Parameter& parameter, ControlKind kind);

// Whether a parameter can be sent by a .kmt template. Kontrol S MK3 templates send CCs, not RPNs or NRPNs.
bool canAssign(const Parameter& parameter);

class Layout {
public:
    std::string name;
    std::string colour = "Violet";
    int midiChannel = 0;  // 0-15
    std::vector<Page> pages{1};
    // The .kmt this layout was opened from, if any, as JSON text: its settings for the keys, wheels, pedals,
    // touch strip and picture are kept when the layout is saved.
    std::string kmtBase;

    Slot& at(SlotId id);
    const Slot& at(SlotId id) const;
    std::optional<SlotId> find(ParamRef ref) const;

    // Puts the parameter in the slot (replacing what is there), taking it out of any other slot.
    void assign(SlotId id, const Parameter& parameter);
    // Puts the parameter in the first empty slot of this kind at or after 'from', adding a page if they're all
    // full, and returns where it went.
    SlotId assignNext(const Parameter& parameter, ControlKind kind, SlotId from);
    void clear(SlotId id);

    // Swaps the slot with the next (delta 1) or previous (delta -1) slot of the same kind, counting across pages:
    // knob 8 of page 1 is followed by knob 1 of page 2. Moving past the last page adds a page. Returns the slot's
    // new place, or nothing if it is already the first.
    std::optional<SlotId> move(SlotId id, int delta);
    // Swaps the slot with the same control on the next or previous page, adding a page after the last.
    std::optional<SlotId> moveToPage(SlotId id, int delta);
    // Moves a parameter from a knob to a button or back: to the same position on the page if that is empty, or the
    // first empty one after it. Returns the new place, or nothing if the slot is empty.
    std::optional<SlotId> switchKind(SlotId id, const Instrument& instrument);

    void addPage();
    // Removes empty pages at the end, keeping at least one page
    void removeEmptyTrailingPages();

private:
    static int linear(SlotId id) { return id.page * kControlsPerRow + id.index; }
    static SlotId fromLinear(int n, ControlKind kind) { return {n / kControlsPerRow, kind, n % kControlsPerRow}; }
    void ensurePages(int count);
};

// The automatic layout for an instrument: a virtual instrument's macros on the knobs in encoder order; otherwise
// its CCs in number order, switches on buttons and the rest on knobs, leaving out bank select, data entry and
// RPN/NRPN numbers, and names that mean a number is unused.
Layout autoLayout(const Instrument& instrument);

}  // namespace kmidi
