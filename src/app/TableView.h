// A text-only table using each platform's *native*, and so accessible, list control (from bcr2000wx):
//   - Windows: wxListCtrl in virtual report mode (the Win32 ListView), which screen readers read.
//   - macOS: wxDataViewCtrl with a virtual list model (an NSTableView), which VoiceOver reads.
// wxListCtrl is custom drawn on macOS, and wxDataViewCtrl is custom drawn on Windows; screen readers can't read
// custom-drawn lists, so each platform uses the other one. Define KMIDI_TABLE_USE_DATAVIEW to build the
// wxDataViewCtrl version on Windows, to check that it compiles and behaves (it won't be accessible there).
#pragma once

#include <wx/panel.h>

#include <functional>
#include <vector>

class TableView : public wxPanel {
public:
    using TextFn = std::function<wxString(long row, int column)>;
    struct Column {
        wxString title;
        int width;  // in DIPs
    };

    TableView(wxWindow* parent, const std::vector<Column>& columns, bool multiSelect, TextFn text,
              const wxString& accessibleName);
    ~TableView() override;

    // Sets the number of rows, keeping the selection where it can
    void setRowCount(long rows);
    long rowCount() const { return rows_; }
    // Redraws every row from TextFn, keeping the selection
    void refreshAll();

    long selectedRow() const;  // the first selected row, or -1
    std::vector<long> selectedRows() const;
    // Selects only this row, makes it the current row and scrolls to it
    void selectRow(long row);
    wxWindow* control() const { return ctrl_; }
    void focus() { ctrl_->SetFocus(); }

    std::function<void()> onActivated;             // Enter or double click
    std::function<bool(const wxKeyEvent&)> onKey;  // return true if handled; called before the control sees the key

private:
    struct Impl;
    Impl* impl_ = nullptr;
    wxWindow* ctrl_ = nullptr;
    long rows_ = 0;
    TextFn text_;
};
