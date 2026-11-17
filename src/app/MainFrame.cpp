#include "MainFrame.h"

#include <wx/button.h>
#include <wx/choicdlg.h>
#include <wx/choice.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "Announce.h"
#include "Kmt.h"
#include "Library.h"
#include "SlotDialog.h"
#include "TableView.h"

using namespace kmidi;

namespace {

enum {
    ID_OpenInstrument = wxID_HIGHEST + 1,
    ID_OpenTemplate,
    ID_Undo,
    ID_Redo,
    ID_AutoLayout,
    ID_PlaceAuto,
    ID_PlaceKnob,
    ID_PlaceButton,
    ID_Edit,
    ID_MoveLeft,
    ID_MoveRight,
    ID_MovePrevPage,
    ID_MoveNextPage,
    ID_SwitchKind,
    ID_Clear,
    ID_AddPage,
    ID_RemoveEmptyPages,
    ID_Shortcuts,
};

const char* const kTemplateWildcard = "Kontrol MIDI templates (*.kmt)|*.kmt|All files (*.*)|*.*";

#ifdef __WXOSX__
// On macOS, Option+arrow keys move by word in text fields, so they aren't menu shortcuts there (which would take
// them from the template name field): the template controls list handles them itself.
const char* const kMoveLeftKey = "";
const char* const kMoveRightKey = "";
const char* const kPrevPageKey = "";
const char* const kNextPageKey = "";
const char* const kRedoKey = "\tCtrl+Shift+Z";  // Cmd+Shift+Z
#else
const char* const kMoveLeftKey = "\tAlt+Left";
const char* const kMoveRightKey = "\tAlt+Right";
const char* const kPrevPageKey = "\tAlt+PgUp";
const char* const kNextPageKey = "\tAlt+PgDn";
const char* const kRedoKey = "\tCtrl+Y";
#endif

wxString u8(const std::string& s) { return wxString::FromUTF8(s); }

wxString rangeText(int minimum, int maximum) { return wxString::Format("%d to %d", minimum, maximum); }

}  // namespace

MainFrame::MainFrame() : wxFrame(nullptr, wxID_ANY, "K-MIDI", wxDefaultPosition, wxSize(1100, 700)) {
    // ---- Menus ----
    auto* fileMenu = new wxMenu;
    fileMenu->Append(ID_OpenInstrument, "&Open instrument...\tCtrl+O",
                     "Open a Cakewalk .ins, Asp .toml or REAPER .reabank file");
    fileMenu->Append(ID_OpenTemplate, "Open &template...\tCtrl+Shift+O", "Open a Kontrol MIDI template (.kmt)");
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_SAVE, "&Save template\tCtrl+S");
    fileMenu->Append(wxID_SAVEAS, "Save template &as...\tCtrl+Shift+S");
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT, "E&xit");

    // Custom IDs, not wxID_UNDO, so the template name field doesn't take the commands for its own undo
    auto* editMenu = new wxMenu;
    editMenu->Append(ID_Undo, "&Undo\tCtrl+Z");
    editMenu->Append(ID_Redo, wxString("&Redo") + kRedoKey);

    auto* controlMenu = new wxMenu;
    controlMenu->Append(ID_PlaceAuto, "&Place parameter\tCtrl+P",
                        "Put the selected parameters on the next empty knobs, or buttons for switches");
    controlMenu->Append(ID_PlaceKnob, "Place parameter on a &knob\tCtrl+K");
    controlMenu->Append(ID_PlaceButton, "Place parameter on a &button\tCtrl+Shift+K");
    controlMenu->AppendSeparator();
    controlMenu->Append(ID_Edit, "&Edit control...\tCtrl+E", "Change the control's label, range and behaviour");
    controlMenu->Append(ID_MoveLeft, wxString("Move &left") + kMoveLeftKey);
    controlMenu->Append(ID_MoveRight, wxString("Move &right") + kMoveRightKey);
    controlMenu->Append(ID_MovePrevPage, wxString("Move to pre&vious page") + kPrevPageKey);
    controlMenu->Append(ID_MoveNextPage, wxString("Move to &next page") + kNextPageKey);
    controlMenu->Append(ID_SwitchKind, "&Switch between knob and button\tCtrl+B");
    controlMenu->Append(ID_Clear, "&Clear control");

    auto* layoutMenu = new wxMenu;
    layoutMenu->Append(ID_AutoLayout, "&Automatic layout\tCtrl+L",
                       "Lay out every parameter again, replacing the current layout");
    layoutMenu->Append(ID_AddPage, "Add &page\tCtrl+Shift+N");
    layoutMenu->Append(ID_RemoveEmptyPages, "&Remove empty pages at the end");

    auto* helpMenu = new wxMenu;
    helpMenu->Append(ID_Shortcuts, "&Keyboard shortcuts\tF1");
    helpMenu->Append(wxID_ABOUT, "&About K-MIDI");

    auto* menuBar = new wxMenuBar;
    menuBar->Append(fileMenu, "&File");
    menuBar->Append(editMenu, "&Edit");
    menuBar->Append(controlMenu, "&Control");
    menuBar->Append(layoutMenu, "&Layout");
    menuBar->Append(helpMenu, "&Help");
    SetMenuBar(menuBar);

    // ---- Template settings ----
    // Each label is created just before its control, which Windows screen readers use as the control's name.
    // VoiceOver doesn't, so each control is also given its name explicitly.
    auto* panel = new wxPanel(this);
    auto* top = new wxBoxSizer(wxVERTICAL);
    auto* fields = new wxBoxSizer(wxHORIZONTAL);
    auto addLabel = [&](const wxString& text) {
        fields->Add(new wxStaticText(panel, wxID_ANY, text), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    };
    addLabel("Template &name:");
    nameText_ = new wxTextCtrl(panel, wxID_ANY);
    fields->Add(nameText_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 16);
    addLabel("M&IDI channel:");
    channelSpin_ = new wxSpinCtrl(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS,
                                  1, 16, 1);
    fields->Add(channelSpin_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 16);
    addLabel("C&olour:");
    wxArrayString colours;
    for (const std::string& c : kColours) colours.Add(u8(c));
    colourChoice_ = new wxChoice(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, colours);
    fields->Add(colourChoice_, 0, wxALIGN_CENTER_VERTICAL);
    top->Add(fields, 0, wxEXPAND | wxALL, 10);
    a11y::setName(nameText_, "Template name");
    a11y::setName(channelSpin_, "MIDI channel");
    a11y::setName(colourChoice_, "Colour");

    // ---- Parameters (left) and template controls (right) ----
    auto* lists = new wxBoxSizer(wxHORIZONTAL);

    auto* left = new wxBoxSizer(wxVERTICAL);
    left->Add(new wxStaticText(panel, wxID_ANY, "&Parameters:"), 0, wxBOTTOM, 4);
    paramTable_ = new TableView(panel, {{"Parameter", 200}, {"Sends", 90}, {"Range", 80}, {"Placed on", 150}}, true,
                                [this](long row, int column) { return paramText(row, column); }, "Parameters");
    left->Add(paramTable_, 1, wxEXPAND);
    auto* paramButtons = new wxBoxSizer(wxHORIZONTAL);
    paramButtons->Add(new wxButton(panel, ID_PlaceKnob, "Place on a &knob"), 0, wxRIGHT, 6);
    paramButtons->Add(new wxButton(panel, ID_PlaceButton, "Place on a b&utton"));
    left->Add(paramButtons, 0, wxTOP, 6);
    lists->Add(left, 1, wxEXPAND | wxRIGHT, 10);

    auto* right = new wxBoxSizer(wxVERTICAL);
    right->Add(new wxStaticText(panel, wxID_ANY, "&Template controls:"), 0, wxBOTTOM, 4);
    slotTable_ = new TableView(panel, {{"Control", 130}, {"Label", 160}, {"Sends", 180}, {"Settings", 200}}, false,
                               [this](long row, int column) { return slotText(row, column); }, "Template controls");
    right->Add(slotTable_, 1, wxEXPAND);
    auto* slotButtons = new wxBoxSizer(wxHORIZONTAL);
    slotButtons->Add(new wxButton(panel, ID_MoveLeft, "Mo&ve left"), 0, wxRIGHT, 6);
    slotButtons->Add(new wxButton(panel, ID_MoveRight, "Move &right"), 0, wxRIGHT, 6);
    slotButtons->Add(new wxButton(panel, ID_SwitchKind, "&Switch knob/button"), 0, wxRIGHT, 6);
    slotButtons->Add(new wxButton(panel, ID_Edit, "E&dit..."), 0, wxRIGHT, 6);
    slotButtons->Add(new wxButton(panel, ID_Clear, "Cle&ar"));
    right->Add(slotButtons, 0, wxTOP, 6);
    lists->Add(right, 1, wxEXPAND);

    top->Add(lists, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    panel->SetSizer(top);
    CreateStatusBar();

    // ---- Events ----
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { onOpenInstrument(); }, ID_OpenInstrument);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { onOpenTemplate(); }, ID_OpenTemplate);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { onSave(false); }, wxID_SAVE);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { onSave(true); }, wxID_SAVEAS);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(); }, wxID_EXIT);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { onUndo(false); }, ID_Undo);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { onUndo(true); }, ID_Redo);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { onPlace(std::nullopt); }, ID_PlaceAuto);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { onPlace(ControlKind::Knob); }, ID_PlaceKnob);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { onPlace(ControlKind::Button); }, ID_PlaceButton);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { onEdit(); }, ID_Edit);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { onMove(-1, false); }, ID_MoveLeft);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { onMove(1, false); }, ID_MoveRight);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { onMove(-1, true); }, ID_MovePrevPage);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { onMove(1, true); }, ID_MoveNextPage);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { onSwitchKind(); }, ID_SwitchKind);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { onClear(); }, ID_Clear);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { onAutoLayout(); }, ID_AutoLayout);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { onAddPage(); }, ID_AddPage);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { onRemoveEmptyPages(); }, ID_RemoveEmptyPages);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { onShortcuts(); }, ID_Shortcuts);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {
        wxMessageBox("K-MIDI 0.1\n\nMakes Native Instruments Kontrol S MK3 MIDI templates (.kmt) from "
                     "Cakewalk .ins, Asp .toml and REAPER .reabank instrument definitions.",
                     "About K-MIDI", wxOK | wxICON_INFORMATION, this);
    }, wxID_ABOUT);
    // The buttons send the same commands as the menus
    Bind(wxEVT_BUTTON, [this](wxCommandEvent& e) {
        wxCommandEvent menu(wxEVT_MENU, e.GetId());
        ProcessEvent(menu);
    });

    paramTable_->onActivated = [this] { onPlace(std::nullopt); };
    paramTable_->onKey = [this](const wxKeyEvent& e) { return onParamKey(e); };
    slotTable_->onActivated = [this] { onEdit(); };
    slotTable_->onKey = [this](const wxKeyEvent& e) { return onSlotKey(e); };

    nameText_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) {
        if (updatingFields_) return;
        kmidi::Layout before = layout_;
        layout_.name = nameText_->GetValue().utf8_string();
        recordChange("Change template name", before, selectedSlot(), true);
    });
    channelSpin_->Bind(wxEVT_SPINCTRL, [this](wxSpinEvent&) {
        if (updatingFields_ || channelSpin_->GetValue() - 1 == layout_.midiChannel) return;
        kmidi::Layout before = layout_;
        layout_.midiChannel = channelSpin_->GetValue() - 1;
        recordChange("Change MIDI channel", before, selectedSlot(), true);
    });
    colourChoice_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        if (updatingFields_) return;
        kmidi::Layout before = layout_;
        layout_.colour = colourChoice_->GetStringSelection().utf8_string();
        recordChange("Change colour", before, selectedSlot());
    });
    Bind(wxEVT_CLOSE_WINDOW, &MainFrame::onClose, this);

    setLayout(layout_);
    SetStatusText("Open an instrument definition (Ctrl+O) to start.");
    paramTable_->focus();
}

// ---------------------------------------------------------------------------------------------
// Keys in the lists
// ---------------------------------------------------------------------------------------------

bool MainFrame::onSlotKey(const wxKeyEvent& e) {
    int key = e.GetKeyCode();
    int modifiers = e.GetModifiers();
    if (modifiers == wxMOD_NONE && (key == WXK_RETURN || key == WXK_NUMPAD_ENTER)) {
        onEdit();
        return true;
    }
    if (modifiers == wxMOD_NONE && (key == WXK_DELETE || key == WXK_BACK || key == WXK_NUMPAD_DELETE)) {
        onClear();
        return true;
    }
    if (modifiers == wxMOD_ALT) {
        switch (key) {
            case WXK_LEFT: case WXK_NUMPAD_LEFT: onMove(-1, false); return true;
            case WXK_RIGHT: case WXK_NUMPAD_RIGHT: onMove(1, false); return true;
            case WXK_PAGEUP: case WXK_NUMPAD_PAGEUP: onMove(-1, true); return true;
            case WXK_PAGEDOWN: case WXK_NUMPAD_PAGEDOWN: onMove(1, true); return true;
            default: break;
        }
    }
    return false;
}

bool MainFrame::onParamKey(const wxKeyEvent& e) {
    // Enter is handled here rather than relying on the list's activation event, which macOS tables don't send
    if (e.GetModifiers() == wxMOD_NONE && (e.GetKeyCode() == WXK_RETURN || e.GetKeyCode() == WXK_NUMPAD_ENTER)) {
        onPlace(std::nullopt);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------------------------
// Opening and saving
// ---------------------------------------------------------------------------------------------

bool MainFrame::confirmDiscard() {
    if (history_.atSaved()) return true;
    int answer = wxMessageBox("Save the changes to this template first?", "K-MIDI",
                              wxYES_NO | wxCANCEL | wxICON_QUESTION, this);
    if (answer == wxCANCEL) return false;
    if (answer == wxYES) return onSave(false);
    return true;
}

void MainFrame::onOpenInstrument() {
    if (!confirmDiscard()) return;
    wxFileDialog dialog(this, "Open instrument definition", wxEmptyString, wxEmptyString, kInstrumentWildcard,
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() == wxID_OK) openInstrumentFile(dialog.GetPath());
}

void MainFrame::openInstrumentFile(const wxString& path) {
    std::string utf8Path = path.utf8_string();
    std::vector<Instrument> instruments;
    try {
        instruments = loadInstrumentFile(utf8Path);
    } catch (const std::exception& e) {
        wxMessageBox(u8(e.what()), "Can't open instrument", wxOK | wxICON_ERROR, this);
        return;
    }
    if (instruments.empty()) {
        wxMessageBox("This file doesn't define any instruments.", "Can't open instrument", wxOK | wxICON_ERROR, this);
        return;
    }

    // An .ins file usually defines many instruments
    Instrument chosen = instruments.front();
    if (instruments.size() > 1) {
        wxArrayString names;
        for (const Instrument& i : instruments) names.Add(u8(i.name));
        wxSingleChoiceDialog choose(this, "Which instrument do you want to make a template for?", "Choose instrument",
                                    names);
        if (choose.ShowModal() != wxID_OK) return;
        chosen = instruments[choose.GetSelection()];
    }

    if (chosen.isVirtual()) {
        std::vector<std::string> unknown;
        auto resolved = resolveFromLibrary(chosen, librarySearchDirs(utf8Path), &unknown);
        if (!resolved) {
            wxMessageBox(u8(chosen.name + " is based on " + chosen.basedOn +
                            ", which isn't in this folder or an Asp instruments folder. Choose the file that "
                            "defines " + chosen.basedOn + "."),
                         "Find base instrument", wxOK | wxICON_INFORMATION, this);
            wxFileDialog dialog(this, u8("Find " + chosen.basedOn), wxFileName(path).GetPath(), wxEmptyString,
                                kInstrumentWildcard, wxFD_OPEN | wxFD_FILE_MUST_EXIST);
            if (dialog.ShowModal() != wxID_OK) return;
            try {
                for (const Instrument& base : loadInstrumentFile(dialog.GetPath().utf8_string()))
                    if (!base.isVirtual() && lower(trim(base.name)) == lower(trim(chosen.basedOn)))
                        resolved = resolveVirtual(chosen, base, &unknown);
            } catch (const std::exception& e) {
                wxMessageBox(u8(e.what()), "Can't open instrument", wxOK | wxICON_ERROR, this);
                return;
            }
            if (!resolved) {
                wxMessageBox(u8("That file doesn't define " + chosen.basedOn + "."), "Can't open instrument",
                             wxOK | wxICON_ERROR, this);
                return;
            }
        }
        if (!unknown.empty()) {
            std::string list;
            for (const std::string& name : unknown) list += "\n" + name;
            wxMessageBox(u8("These macros name parameters that " + chosen.basedOn +
                            " doesn't have, so they were left out:" + list),
                         "Unknown macros", wxOK | wxICON_WARNING, this);
        }
        chosen = *resolved;
    }

    templatePath_.clear();
    setInstrument(chosen);
    setLayout(autoLayout(chosen));

    int ccs = 0, others = 0, placed = 0;
    for (const Parameter& p : instrument_.parameters) {
        (canAssign(p) ? ccs : others)++;
        if (layout_.find(p.ref())) ++placed;
    }
    if (instrument_.parameters.empty() && !instrument_.presets.empty()) {
        wxMessageBox(wxString::Format(
                         "%s names %zu presets but no controllers, so there is nothing to put on the knobs.\n\n"
                         "Kontrol S MK3 templates can send program changes from buttons, but K-MIDI doesn't "
                         "write them yet: no sample template with a program change button has been found to show how "
                         "a .kmt file stores one.",
                         u8(instrument_.name), instrument_.presets.size()),
                     "No controllers", wxOK | wxICON_INFORMATION, this);
    }
    say(wxString::Format("Opened %s: %d controllers placed on %zu pages.", u8(instrument_.name), placed,
                         layout_.pages.size()));
    if (others > 0)
        SetStatusText(wxString::Format("%d CCs. %d RPNs and NRPNs are listed but can't be placed: Kontrol S MK3 "
                                       "templates only send CCs.", ccs, others));
    paramTable_->focus();
}

void MainFrame::onOpenTemplate() {
    if (!confirmDiscard()) return;
    wxFileDialog dialog(this, "Open MIDI template", wxEmptyString, wxEmptyString, kTemplateWildcard,
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() == wxID_OK) openTemplateFile(dialog.GetPath());
}

void MainFrame::openTemplateFile(const wxString& path) {
    try {
        kmidi::Layout layout = loadKmt(path.utf8_string());
        templatePath_ = path;
        setLayout(layout);
        say(wxString::Format("Opened template %s with %zu pages.", u8(layout_.name), layout_.pages.size()));
        if (instrument_.parameters.empty())
            SetStatusText("To place more parameters, open the template's instrument first, then the template.");
    } catch (const std::exception& e) {
        wxMessageBox(u8(e.what()), "Can't open template", wxOK | wxICON_ERROR, this);
    }
}

bool MainFrame::onSave(bool saveAs) {
    wxString path = templatePath_;
    if (saveAs || path.empty()) {
        wxString name = u8(layout_.name);
        for (wxChar bad : wxString("\\/:*?\"<>|")) name.Replace(wxString(bad), "-");
        wxFileDialog dialog(this, "Save MIDI template", wxEmptyString, name + ".kmt", kTemplateWildcard,
                            wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (dialog.ShowModal() != wxID_OK) return false;
        path = dialog.GetPath();
        if (wxFileName(path).GetExt().empty()) path += ".kmt";
    }
    std::vector<std::string> warnings;
    try {
        saveKmt(path.utf8_string(), layout_, instrument_, &warnings);
    } catch (const std::exception& e) {
        wxMessageBox(u8(e.what()), "Can't save template", wxOK | wxICON_ERROR, this);
        return false;
    }
    templatePath_ = path;
    history_.markSaved();
    updateState();
    if (!warnings.empty()) {
        wxString text = "The template was saved, but:";
        for (const std::string& w : warnings) text += "\n" + u8(w);
        wxMessageBox(text, "Saved with warnings", wxOK | wxICON_WARNING, this);
    } else {
        say("Saved " + wxFileName(path).GetFullName());
    }
    SetStatusText("Saved " + path);
    return true;
}

void MainFrame::onClose(wxCloseEvent& event) {
    if (event.CanVeto() && !confirmDiscard()) {
        event.Veto();
        return;
    }
    Destroy();
}

// ---------------------------------------------------------------------------------------------
// Undo
// ---------------------------------------------------------------------------------------------

void MainFrame::recordChange(const std::string& description, const kmidi::Layout& before,
                             std::optional<SlotId> selection, bool merge) {
    history_.record(description, before, selection, merge);
    updateState();
}

void MainFrame::onUndo(bool redo) {
    if (redo ? !history_.canRedo() : !history_.canUndo()) {
        say(redo ? "Nothing to redo." : "Nothing to undo.");
        return;
    }
    std::string description = redo ? history_.redoDescription() : history_.undoDescription();
    HistoryEntry entry = redo ? history_.redo(layout_, selectedSlot()) : history_.undo(layout_, selectedSlot());
    layout_ = entry.layout;
    refreshAll();
    // The selection goes back to where it was, without moving the focus (which may be in the name field)
    if (entry.selection) selectSlot(*entry.selection, false);
    updateState();
    say(u8((redo ? "Redo " : "Undo ") + description));
}

// ---------------------------------------------------------------------------------------------
// Editing the layout
// ---------------------------------------------------------------------------------------------

void MainFrame::onAutoLayout() {
    if (instrument_.parameters.empty()) {
        wxMessageBox("Open an instrument first.", "Automatic layout", wxOK | wxICON_INFORMATION, this);
        return;
    }
    if (wxMessageBox("Replace the whole layout with the automatic one? You can undo this.", "Automatic layout",
                     wxYES_NO | wxICON_QUESTION, this) != wxYES)
        return;
    kmidi::Layout before = layout_;
    auto selection = selectedSlot();
    kmidi::Layout layout = autoLayout(instrument_);
    layout.name = layout_.name;
    layout.colour = layout_.colour;
    layout.midiChannel = layout_.midiChannel;
    layout.kmtBase = layout_.kmtBase;
    layout_ = layout;
    refreshAll();
    recordChange("Automatic layout", before, selection);
    say("Laid out again.");
}

void MainFrame::onPlace(std::optional<ControlKind> kind) {
    std::vector<const Parameter*> parameters = selectedParameters();
    if (parameters.empty()) {
        say("Select a parameter in the parameters list first.");
        return;
    }
    kmidi::Layout before = layout_;
    std::optional<SlotId> from = selectedSlot();
    SlotId start = from.value_or(SlotId{0, ControlKind::Knob, 0});
    std::optional<SlotId> last;
    std::vector<std::string> refused;
    std::vector<std::string> placedNames;
    wxString placedText;
    for (const Parameter* p : parameters) {
        if (!canAssign(*p)) {
            refused.push_back(std::string(kindName(p->kind)) + " " + std::to_string(p->number) + " " + p->name);
            continue;
        }
        ControlKind k = kind.value_or(isToggle(*p) ? ControlKind::Button : ControlKind::Knob);
        SlotId id = layout_.assignNext(*p, k, start.kind == k ? start : SlotId{start.page, k, 0});
        start = id;
        last = id;
        placedNames.push_back(p->name);
        placedText = u8(p->name + " is on " + describe(id) + ".");
    }
    if (last) {
        refreshAll();
        selectSlot(*last, false);
        recordChange(placedNames.size() == 1 ? "Place " + placedNames.front()
                                              : "Place " + std::to_string(placedNames.size()) + " parameters",
                     before, from);
        say(placedNames.size() == 1 ? placedText : wxString::Format("Placed %zu parameters.", placedNames.size()));
    }
    if (!refused.empty()) {
        wxString text = "Kontrol S MK3 templates can only send CCs, so these weren't placed:";
        for (const std::string& r : refused) text += "\n" + u8(r);
        wxMessageBox(text, "Can't place parameter", wxOK | wxICON_INFORMATION, this);
    }
}

void MainFrame::onEdit() {
    auto id = selectedSlot();
    if (!id) return;
    const Slot& slot = layout_.at(*id);
    if (slot.empty()) {
        say("This control is empty. Place a parameter on it first.");
        return;
    }
    SlotDialog dialog(this, *id, slot, instrument_.find(*slot.param));
    if (dialog.ShowModal() != wxID_OK || dialog.slot() == slot) return;
    kmidi::Layout before = layout_;
    layout_.at(*id) = dialog.slot();
    refreshAll();
    selectSlot(*id, true);
    recordChange("Edit " + before.at(*id).label, before, id);
}

void MainFrame::onMove(int delta, bool byPage) {
    auto id = selectedSlot();
    if (!id) return;
    if (layout_.at(*id).empty()) {
        say("This control is empty.");
        return;
    }
    kmidi::Layout before = layout_;
    auto to = byPage ? layout_.moveToPage(*id, delta) : layout_.move(*id, delta);
    if (!to) {
        say(byPage ? "Already on the first page." : "Already the first control.");
        return;
    }
    refreshAll();
    selectSlot(*to, true);
    std::string label = layout_.at(*to).label;
    recordChange(byPage ? "Move " + label + (delta < 0 ? " to the previous page" : " to the next page")
                        : "Move " + label + (delta < 0 ? " left" : " right"),
                 before, id);
}

void MainFrame::onSwitchKind() {
    auto id = selectedSlot();
    if (!id) return;
    kmidi::Layout before = layout_;
    auto to = layout_.switchKind(*id, instrument_);
    if (!to) {
        say("This control is empty.");
        return;
    }
    refreshAll();
    selectSlot(*to, true);
    recordChange("Switch " + layout_.at(*to).label + (to->kind == ControlKind::Knob ? " to a knob" : " to a button"),
                 before, id);
}

void MainFrame::onClear() {
    auto id = selectedSlot();
    if (!id || layout_.at(*id).empty()) return;
    kmidi::Layout before = layout_;
    std::string label = layout_.at(*id).label;
    layout_.clear(*id);
    refreshAll();
    selectSlot(*id, true);
    recordChange("Clear " + label, before, id);
    say(u8("Cleared " + label));
}

void MainFrame::onAddPage() {
    kmidi::Layout before = layout_;
    auto selection = selectedSlot();
    layout_.addPage();
    refreshAll();
    selectSlot({static_cast<int>(layout_.pages.size()) - 1, ControlKind::Knob, 0}, true);
    recordChange("Add page", before, selection);
    say(wxString::Format("Added page %zu.", layout_.pages.size()));
}

void MainFrame::onRemoveEmptyPages() {
    kmidi::Layout before = layout_;
    auto selection = selectedSlot();
    layout_.removeEmptyTrailingPages();
    size_t removed = before.pages.size() - layout_.pages.size();
    if (removed) {
        refreshAll();
        recordChange("Remove empty pages", before, selection);
    }
    say(wxString::Format("Removed %zu pages. There are %zu.", removed, layout_.pages.size()));
}

void MainFrame::onShortcuts() {
    wxString text =
        "In the parameters list:\n"
        "  Enter or Ctrl+P: place the selected parameters on the next empty knobs (buttons for switches), "
        "after the control selected in the template list\n"
        "  Ctrl+K: place on knobs.  Ctrl+Shift+K: place on buttons\n\n"
        "In the template controls list (each page lists knobs 1 to 8, then buttons 1 to 8):\n"
        "  Enter or Ctrl+E: edit the control's label and range\n"
        "  Alt+Left, Alt+Right: move the control left or right, swapping it with the one it moves over; "
        "knob 8 moves on to knob 1 of the next page\n"
        "  Alt+Page Up, Alt+Page Down: move it to the previous or next page\n"
        "  Ctrl+B: switch it between a knob and a button\n"
        "  Delete: clear it\n\n"
        "Ctrl+Z: undo.  Ctrl+Y: redo.\n"
        "Ctrl+L: automatic layout.  Ctrl+Shift+N: add a page.\n"
        "Ctrl+O: open an instrument.  Ctrl+Shift+O: open a template.  Ctrl+S: save the template.";
#ifdef __WXOSX__
    // On a Mac, Ctrl in these shortcuts is Command and Alt is Option; redo is Command+Shift+Z
    text.Replace("Ctrl+Y", "Ctrl+Shift+Z");
    text.Replace("Ctrl+", "Command+");
    text.Replace("Alt+", "Option+");
#endif
    wxMessageBox(text, "Keyboard shortcuts", wxOK | wxICON_INFORMATION, this);
}

// ---------------------------------------------------------------------------------------------
// Showing the layout
// ---------------------------------------------------------------------------------------------

void MainFrame::setInstrument(const Instrument& instrument) {
    instrument_ = instrument;
    paramTable_->setRowCount(static_cast<long>(instrument_.parameters.size()));
    paramTable_->refreshAll();
    if (!instrument_.parameters.empty()) paramTable_->selectRow(0);
}

void MainFrame::setLayout(const kmidi::Layout& layout) {
    layout_ = layout;
    history_.clear();
    refreshAll();
    if (!slotRows_.empty()) selectSlot(slotRows_.front(), false);
    updateState();
}

void MainFrame::refreshAll() {
    refreshTemplateFields();
    paramTable_->refreshAll();
    refreshSlots();
}

void MainFrame::refreshTemplateFields() {
    updatingFields_ = true;
    // ChangeValue keeps the cursor where it is when the text hasn't changed
    if (nameText_->GetValue() != u8(layout_.name)) nameText_->ChangeValue(u8(layout_.name));
    channelSpin_->SetValue(layout_.midiChannel + 1);
    int colour = colourChoice_->FindString(u8(layout_.colour));
    if (colour == wxNOT_FOUND) colour = colourChoice_->Append(u8(layout_.colour));
    colourChoice_->SetSelection(colour);
    updatingFields_ = false;
}

void MainFrame::refreshSlots() {
    slotRows_.clear();
    for (int p = 0; p < static_cast<int>(layout_.pages.size()); ++p)
        for (ControlKind kind : {ControlKind::Knob, ControlKind::Button})
            for (int i = 0; i < kControlsPerRow; ++i) slotRows_.push_back({p, kind, i});
    if (slotTable_->rowCount() != static_cast<long>(slotRows_.size()))
        slotTable_->setRowCount(static_cast<long>(slotRows_.size()));
    slotTable_->refreshAll();
}

wxString MainFrame::paramText(long row, int column) const {
    if (row < 0 || row >= static_cast<long>(instrument_.parameters.size())) return {};
    const Parameter& p = instrument_.parameters[row];
    switch (column) {
        case 0: return u8(p.name);
        case 1: return wxString::Format("%s %d", kindName(p.kind), p.number);
        case 2:
            return p.valueNames.empty() ? rangeText(p.minimum, p.maximum)
                                        : wxString::Format("%zu values", p.valueNames.size());
        case 3:
            if (!canAssign(p)) return "Can't be sent";
            if (auto id = layout_.find(p.ref())) return u8(describe(*id));
            return {};
        default: return {};
    }
}

wxString MainFrame::slotText(long row, int column) const {
    if (row < 0 || row >= static_cast<long>(slotRows_.size())) return {};
    SlotId id = slotRows_[row];
    if (id.page >= static_cast<int>(layout_.pages.size())) return {};
    const Slot& slot = layout_.at(id);
    switch (column) {
        case 0: return u8(describe(id));
        case 1: return slot.empty() ? wxString() : u8(slot.label);
        case 2: return slotParameterText(slot);
        case 3: return slotSettingsText(id, slot);
        default: return {};
    }
}

wxString MainFrame::slotParameterText(const Slot& slot) const {
    if (slot.empty()) return "Empty";
    wxString text = wxString::Format("%s %d", kindName(slot.param->kind), slot.param->number);
    if (const Parameter* p = instrument_.find(*slot.param))
        if (p->name != slot.label) text += u8(" (" + p->name + ")");
    return text;
}

wxString MainFrame::slotSettingsText(SlotId id, const Slot& slot) const {
    if (slot.empty()) return {};
    if (id.kind == ControlKind::Knob)
        return rangeText(slot.minimum, slot.maximum) + (slot.display == KnobDisplay::Percent ? ", percent" : "");
    if (slot.mode == ButtonMode::Increment)
        return wxString::Format("Steps %d to %d by %d%s, %s", slot.minimum, slot.maximum, slot.step,
                                slot.wrap ? " and wraps" : "", u8(slot.colour));
    return wxString::Format("Toggles %d and %d, %s", slot.minimum, slot.maximum, u8(slot.colour));
}

void MainFrame::selectSlot(SlotId id, bool focus) {
    for (size_t row = 0; row < slotRows_.size(); ++row) {
        if (!(slotRows_[row] == id)) continue;
        slotTable_->selectRow(static_cast<long>(row));
        if (focus) slotTable_->focus();
        return;
    }
}

std::optional<SlotId> MainFrame::selectedSlot() const {
    long row = slotTable_->selectedRow();
    if (row < 0 || row >= static_cast<long>(slotRows_.size())) return std::nullopt;
    return slotRows_[row];
}

std::vector<const Parameter*> MainFrame::selectedParameters() const {
    std::vector<const Parameter*> result;
    for (long row : paramTable_->selectedRows())
        if (row < static_cast<long>(instrument_.parameters.size())) result.push_back(&instrument_.parameters[row]);
    return result;
}

void MainFrame::updateState() {
    wxString title = "K-MIDI";
    if (!layout_.name.empty())
        title = u8(layout_.name) + (history_.atSaved() ? "" : " (changed)") + " - " + title;
    SetTitle(title);

    if (wxMenuBar* bar = GetMenuBar()) {
        wxString undo = history_.canUndo() ? "&Undo " + u8(history_.undoDescription()) : wxString("&Undo");
        wxString redo = history_.canRedo() ? "&Redo " + u8(history_.redoDescription()) : wxString("&Redo");
        // A "&" in a label would be read as an access key, so it is doubled
        auto escape = [](wxString s, size_t keep) {
            wxString head = s.Left(keep), tail = s.Mid(keep);
            tail.Replace("&", "&&");
            return head + tail;
        };
        bar->SetLabel(ID_Undo, escape(undo, 5) + "\tCtrl+Z");
        bar->SetLabel(ID_Redo, escape(redo, 5) + kRedoKey);
        // Undo and Redo stay enabled, so pressing them with nothing to do says so rather than doing nothing
    }
}

void MainFrame::say(const wxString& text) {
    SetStatusText(text);
    a11y::announce(this, text);
}
