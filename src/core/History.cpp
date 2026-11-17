#include "History.h"

#include <utility>

namespace kmidi {

namespace {
const std::string kNothing;
}

void History::clear() {
    undo_.clear();
    redo_.clear();
    position_ = savedPosition_ = 0;
    canMerge_ = false;
}

void History::record(const std::string& description, const Layout& before, std::optional<SlotId> selection,
                     bool merge) {
    // A new change means what was undone can't be redone, so a saved state among it can't be reached again
    if (!redo_.empty() && savedPosition_ > position_) savedPosition_ = -1;
    redo_.clear();
    bool join = merge && canMerge_ && !undo_.empty() && undo_.back().description == description;
    canMerge_ = true;
    if (join) return;  // the last entry already holds the layout from before these changes
    undo_.push_back({description, before, selection});
    ++position_;
}

const std::string& History::undoDescription() const { return undo_.empty() ? kNothing : undo_.back().description; }

const std::string& History::redoDescription() const { return redo_.empty() ? kNothing : redo_.back().description; }

HistoryEntry History::undo(const Layout& current, std::optional<SlotId> currentSelection) {
    HistoryEntry entry = std::move(undo_.back());
    undo_.pop_back();
    redo_.push_back({entry.description, current, currentSelection});
    --position_;
    canMerge_ = false;
    return entry;
}

HistoryEntry History::redo(const Layout& current, std::optional<SlotId> currentSelection) {
    HistoryEntry entry = std::move(redo_.back());
    redo_.pop_back();
    undo_.push_back({entry.description, current, currentSelection});
    ++position_;
    canMerge_ = false;
    return entry;
}

}  // namespace kmidi
