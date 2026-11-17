// The main window: the instrument's parameters on the left, the template's pages of knobs and buttons on the right.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include <wx/frame.h>

#include "History.h"
#include "Instrument.h"
#include "Layout.h"

class TableView;
class wxTextCtrl;
class wxSpinCtrl;
class wxChoice;
class wxKeyEvent;

class MainFrame : public wxFrame {
public:
    MainFrame();
    void openInstrumentFile(const wxString& path);
    void openTemplateFile(const wxString& path);

private:
    // Commands
    void onOpenInstrument();
    void onOpenTemplate();
    bool onSave(bool saveAs);
    void onUndo(bool redo);
    void onAutoLayout();
    void onPlace(std::optional<kmidi::ControlKind> kind);  // nothing: knob or button, whichever suits
    void onEdit();
    void onMove(int delta, bool byPage);
    void onSwitchKind();
    void onClear();
    void onAddPage();
    void onRemoveEmptyPages();
    void onShortcuts();
    void onClose(wxCloseEvent& event);
    bool onSlotKey(const wxKeyEvent& event);
    bool onParamKey(const wxKeyEvent& event);

    // Records a change for undo, given the layout and selected control from before it
    void recordChange(const std::string& description, const kmidi::Layout& before, std::optional<kmidi::SlotId> selection,
                      bool merge = false);

    // Showing the layout
    void setInstrument(const kmidi::Instrument& instrument);
    void setLayout(const kmidi::Layout& layout);  // a newly opened layout: forgets the undo history
    void refreshAll();
    void refreshTemplateFields();
    void refreshSlots();
    void selectSlot(kmidi::SlotId id, bool focus);
    std::optional<kmidi::SlotId> selectedSlot() const;
    std::vector<const kmidi::Parameter*> selectedParameters() const;
    wxString paramText(long row, int column) const;
    wxString slotText(long row, int column) const;
    wxString slotParameterText(const kmidi::Slot& slot) const;
    wxString slotSettingsText(kmidi::SlotId id, const kmidi::Slot& slot) const;
    void updateState();  // the title's "(changed)" and the Undo and Redo menu items
    bool confirmDiscard();
    void say(const wxString& text);

    kmidi::Instrument instrument_;
    kmidi::Layout layout_;
    kmidi::History history_;
    wxString templatePath_;
    bool updatingFields_ = false;

    wxTextCtrl* nameText_ = nullptr;
    wxSpinCtrl* channelSpin_ = nullptr;
    wxChoice* colourChoice_ = nullptr;
    TableView* paramTable_ = nullptr;
    TableView* slotTable_ = nullptr;
    std::vector<kmidi::SlotId> slotRows_;
};
