#include "SlotDialog.h"

#include <wx/menuitem.h>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "Announce.h"

using namespace kmidi;

SlotDialog::SlotDialog(wxWindow* parent, SlotId id, const Slot& slot, const Parameter* parameter)
    : wxDialog(parent, wxID_ANY, wxString::FromUTF8("Edit " + describe(id))), id_(id), slot_(slot) {
    auto* top = new wxBoxSizer(wxVERTICAL);

    // What the control sends, for reference
    wxString sends;
    if (slot.param) {
        sends = wxString::Format("Sends %s %d", kindName(slot.param->kind), slot.param->number);
        if (parameter) sends += wxString::FromUTF8(" (" + parameter->name + ")");
        if (parameter && !parameter->valueNames.empty()) {
            wxString names;
            for (const auto& [value, name] : parameter->valueNames)
                names += wxString::Format("%s%d %s", names.empty() ? "" : ", ", value, wxString::FromUTF8(name));
            sends += ". Values: " + names;
        }
    }
    if (!sends.empty()) top->Add(new wxStaticText(this, wxID_ANY, sends), 0, wxALL, 10);

    auto* grid = new wxFlexGridSizer(2, 6, 10);
    grid->AddGrowableCol(1);
    // Each label is created just before its control, so screen readers name the control after it
    auto addRow = [&](const wxString& text, wxWindow* (*make)(wxWindow*)) {
        grid->Add(new wxStaticText(this, wxID_ANY, text), 0, wxALIGN_CENTER_VERTICAL);
        wxWindow* control = make(this);
        grid->Add(control, 1, wxEXPAND);
        // VoiceOver doesn't take a control's name from the label beside it, so it is set explicitly
        wxString name = wxStripMenuCodes(text);
        if (name.EndsWith(":")) name.RemoveLast();
        a11y::setName(control, name);
        return control;
    };

    label_ = static_cast<wxTextCtrl*>(addRow("&Label:", [](wxWindow* p) -> wxWindow* { return new wxTextCtrl(p, wxID_ANY); }));
    label_->SetValue(wxString::FromUTF8(slot.label));
    label_->SetMinSize(wxSize(250, -1));

    bool knob = id.kind == ControlKind::Knob;
    if (!knob) {
        mode_ = static_cast<wxChoice*>(addRow("&When pressed:", [](wxWindow* p) -> wxWindow* {
            wxString choices[] = {"Toggle between the lowest and highest values", "Step through the values"};
            return new wxChoice(p, wxID_ANY, wxDefaultPosition, wxDefaultSize, 2, choices);
        }));
        mode_->SetSelection(slot.mode == ButtonMode::Increment ? 1 : 0);
    }
    auto makeSpin = [](wxWindow* p) -> wxWindow* {
        return new wxSpinCtrl(p, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 127, 0);
    };
    minimum_ = static_cast<wxSpinCtrl*>(addRow(knob ? "L&owest value:" : "L&owest (off) value:", makeSpin));
    minimum_->SetValue(slot.minimum);
    maximum_ = static_cast<wxSpinCtrl*>(addRow(knob ? "&Highest value:" : "&Highest (on) value:", makeSpin));
    maximum_->SetValue(slot.maximum);
    if (knob) {
        display_ = static_cast<wxChoice*>(addRow("&Show value as:", [](wxWindow* p) -> wxWindow* {
            wxString choices[] = {"Whole number", "Percent"};
            return new wxChoice(p, wxID_ANY, wxDefaultPosition, wxDefaultSize, 2, choices);
        }));
        display_->SetSelection(slot.display == KnobDisplay::Percent ? 1 : 0);
    } else {
        step_ = static_cast<wxSpinCtrl*>(addRow("S&tep:", [](wxWindow* p) -> wxWindow* {
            return new wxSpinCtrl(p, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1,
                                  127, 1);
        }));
        step_->SetValue(slot.step);
        grid->AddSpacer(0);
        wrap_ = new wxCheckBox(this, wxID_ANY, "&Go back to the lowest value after the highest");
        wrap_->SetValue(slot.wrap);
        grid->Add(wrap_);
        colour_ = static_cast<wxChoice*>(addRow("&Colour:", [](wxWindow* p) -> wxWindow* {
            wxArrayString names;
            for (const std::string& c : kColours) names.Add(wxString::FromUTF8(c));
            return new wxChoice(p, wxID_ANY, wxDefaultPosition, wxDefaultSize, names);
        }));
        int colour = colour_->FindString(wxString::FromUTF8(slot.colour));
        colour_->SetSelection(colour == wxNOT_FOUND ? 0 : colour);
        mode_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { updateEnabled(); });
    }

    top->Add(grid, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    top->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 10);
    SetSizerAndFit(top);
    updateEnabled();
    label_->SetFocus();
    label_->SelectAll();
    CentreOnParent();
}

void SlotDialog::updateEnabled() {
    if (!mode_) return;
    bool increment = mode_->GetSelection() == 1;
    step_->Enable(increment);
    wrap_->Enable(increment);
}

bool SlotDialog::TransferDataFromWindow() {
    if (minimum_->GetValue() > maximum_->GetValue()) {
        wxMessageBox("The lowest value must not be above the highest value.", GetTitle(), wxOK | wxICON_WARNING, this);
        minimum_->SetFocus();
        return false;
    }
    slot_.label = label_->GetValue().utf8_string();
    slot_.minimum = minimum_->GetValue();
    slot_.maximum = maximum_->GetValue();
    if (display_) slot_.display = display_->GetSelection() == 1 ? KnobDisplay::Percent : KnobDisplay::Integer;
    if (mode_) {
        slot_.mode = mode_->GetSelection() == 1 ? ButtonMode::Increment : ButtonMode::Toggle;
        slot_.step = step_->GetValue();
        slot_.wrap = wrap_->GetValue();
        slot_.colour = colour_->GetStringSelection().utf8_string();
    }
    return true;
}

Slot SlotDialog::slot() const { return slot_; }
