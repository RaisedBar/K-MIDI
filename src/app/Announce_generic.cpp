// Platforms without screen reader helpers yet (macOS would use NSAccessibility).
#include "Announce.h"

namespace a11y {

void announce(wxWindow*, const wxString&) {}

void setName(wxWindow*, const wxString&) {}

}  // namespace a11y
