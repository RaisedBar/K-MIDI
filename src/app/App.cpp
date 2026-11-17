// K-MIDI: makes Kontrol S MK3 MIDI templates (.kmt) from instrument definitions.
// Run with a file to open it: an instrument definition (.ins, .toml, .reabank) or a template (.kmt).
#include <wx/app.h>
#include <wx/filename.h>

#include "MainFrame.h"

class App : public wxApp {
public:
    bool OnInit() override {
        // wxApp::OnInit isn't called: it would parse the command line and reject the file names
        SetAppDisplayName("K-MIDI");
        auto* frame = new MainFrame;
        frame->Show();
        for (int i = 1; i < argc; ++i) {
            wxString path = argv[i];
            if (wxFileName(path).GetExt().Lower() == "kmt")
                frame->openTemplateFile(path);
            else
                frame->openInstrumentFile(path);
        }
        return true;
    }
};

wxIMPLEMENT_APP(App);
