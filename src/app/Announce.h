// Screen reader helpers.
#pragma once

#include <wx/window.h>

namespace a11y {

// Asks the screen reader to speak a message without moving focus.
// Windows: a UI Automation notification event (Narrator, NVDA, recent JAWS). Elsewhere: does nothing.
void announce(wxWindow* source, const wxString& text);

// Gives a control without text of its own (such as a list) an accessible name.
// Windows: MSAA dynamic annotation, which leaves the control's own accessibility (its list items) untouched.
void setName(wxWindow* window, const wxString& name);

}  // namespace a11y
