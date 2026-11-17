// Windows implementation of a11y::setName() using MSAA dynamic annotation (IAccPropServices).
// Kept apart from Announce_msw.cpp: <initguid.h> here defines the PROPID_ACC_* GUIDs, and including
// <uiautomation.h> in the same file would define the UIA GUIDs a second time.
#include "Announce.h"

#include <windows.h>
#include <initguid.h>
#include <oleacc.h>

namespace a11y {

void setName(wxWindow* window, const wxString& name) {
    if (!window) return;
    static IAccPropServices* services = [] {
        IAccPropServices* p = nullptr;
        if (FAILED(CoCreateInstance(__uuidof(CAccPropServices), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&p))))
            p = nullptr;
        return p;
    }();
    if (!services) return;
    services->SetHwndPropStr(static_cast<HWND>(window->GetHWND()), static_cast<DWORD>(OBJID_CLIENT), CHILDID_SELF,
                             PROPID_ACC_NAME, name.wc_str());
}

}  // namespace a11y
