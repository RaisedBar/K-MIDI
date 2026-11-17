// Undo and redo for a layout. Each change is recorded as the layout before it (a template is small, so whole
// copies are simplest), with a description such as "Move Cutoff right" and the control that was selected.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Layout.h"

namespace kmidi {

struct HistoryEntry {
    std::string description;
    Layout layout;
    std::optional<SlotId> selection;
};

class History {
public:
    // Forgets every change, and counts the current layout as saved
    void clear();

    // Records a change, given the layout and selection from before it. With merge, a change with the same
    // description as the last one (typing in the name field, say) is joined to it, so one undo undoes both,
    // unless the layout was saved in between.
    void record(const std::string& description, const Layout& before, std::optional<SlotId> selection,
                bool merge = false);

    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    const std::string& undoDescription() const;  // "" if there is nothing to undo
    const std::string& redoDescription() const;

    // Returns the layout and selection to go back to, keeping the current ones for redo. Only call if canUndo().
    HistoryEntry undo(const Layout& current, std::optional<SlotId> currentSelection);
    // Returns the layout and selection to go forward to. Only call if canRedo().
    HistoryEntry redo(const Layout& current, std::optional<SlotId> currentSelection);

    // Whether the current layout is the one last saved (or opened)
    bool atSaved() const { return position_ == savedPosition_; }
    void markSaved() {
        savedPosition_ = position_;
        canMerge_ = false;
    }

private:
    std::vector<HistoryEntry> undo_, redo_;
    int position_ = 0;       // changes made, less those undone
    int savedPosition_ = 0;  // position_ when saved, or -1 if that state can't be reached any more
    bool canMerge_ = false;  // the last thing done was record(), so another change can join it
};

}  // namespace kmidi
