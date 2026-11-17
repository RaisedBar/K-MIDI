// Edits one knob or button of the template: its label, range, and how it shows or sends its value.
#pragma once

#include <wx/dialog.h>

#include "Instrument.h"
#include "Layout.h"

class wxTextCtrl;
class wxSpinCtrl;
class wxChoice;
class wxCheckBox;
class wxStaticText;

class SlotDialog : public wxDialog {
public:
    // parameter may be null (a template opened without its instrument)
    SlotDialog(wxWindow* parent, kmidi::SlotId id, const kmidi::Slot& slot, const kmidi::Parameter* parameter);
    kmidi::Slot slot() const;

private:
    void updateEnabled();
    bool TransferDataFromWindow() override;

    kmidi::SlotId id_;
    kmidi::Slot slot_;
    wxTextCtrl* label_ = nullptr;
    wxSpinCtrl* minimum_ = nullptr;
    wxSpinCtrl* maximum_ = nullptr;
    wxChoice* display_ = nullptr;  // knobs
    wxChoice* mode_ = nullptr;     // buttons
    wxSpinCtrl* step_ = nullptr;
    wxCheckBox* wrap_ = nullptr;
    wxChoice* colour_ = nullptr;
};
