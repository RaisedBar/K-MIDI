// Windows implementation of a11y::announce() using UI Automation notification events.
#include "Announce.h"

#include <windows.h>
#include <uiautomation.h>

namespace a11y {

void announce(wxWindow* source, const wxString& text) {
    if (!source || text.empty()) return;
    wxWindow* top = wxGetTopLevelParent(source);
    HWND hwnd = static_cast<HWND>((top ? top : source)->GetHWND());
    IRawElementProviderSimple* provider = nullptr;
    if (FAILED(UiaHostProviderFromHwnd(hwnd, &provider)) || !provider) return;
    BSTR display = SysAllocString(text.wc_str());
    BSTR activity = SysAllocString(L"K-MIDI.Announcement");
    UiaRaiseNotificationEvent(provider, NotificationKind_Other, NotificationProcessing_ImportantMostRecent, display,
                              activity);
    SysFreeString(display);
    SysFreeString(activity);
    provider->Release();
}

}  // namespace a11y
