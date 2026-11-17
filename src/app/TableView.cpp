#include "TableView.h"

#include <wx/sizer.h>

#include <algorithm>

#include "Announce.h"

#if defined(__WXMSW__) && !defined(KMIDI_TABLE_USE_DATAVIEW)
#define KMIDI_TABLE_LISTCTRL 1
#include <wx/listctrl.h>
#else
#define KMIDI_TABLE_LISTCTRL 0
#include <wx/dataview.h>
#endif

#if KMIDI_TABLE_LISTCTRL
// ---------------------------------------------------------------------------------------------
// Windows: virtual wxListCtrl (native ListView)
// ---------------------------------------------------------------------------------------------
namespace {
class VirtualList : public wxListCtrl {
public:
    VirtualList(wxWindow* parent, bool multi, TableView::TextFn* text)
        : wxListCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                     wxLC_REPORT | wxLC_VIRTUAL | (multi ? 0 : wxLC_SINGLE_SEL)),
          text_(text) {}
    wxString OnGetItemText(long item, long column) const override { return (*text_)(item, static_cast<int>(column)); }

private:
    TableView::TextFn* text_;
};
}  // namespace

struct TableView::Impl {
    VirtualList* list = nullptr;
};

TableView::TableView(wxWindow* parent, const std::vector<Column>& columns, bool multiSelect, TextFn text,
                     const wxString& accessibleName)
    : wxPanel(parent), impl_(new Impl), text_(std::move(text)) {
    impl_->list = new VirtualList(this, multiSelect, &text_);
    for (const Column& c : columns) impl_->list->AppendColumn(c.title, wxLIST_FORMAT_LEFT, FromDIP(c.width));
    ctrl_ = impl_->list;
    impl_->list->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent&) {
        if (onActivated) onActivated();
    });

#else
// ---------------------------------------------------------------------------------------------
// macOS: wxDataViewCtrl with a virtual list model (native NSTableView)
// ---------------------------------------------------------------------------------------------
namespace {
class VirtualModel : public wxDataViewVirtualListModel {
public:
    explicit VirtualModel(TableView::TextFn* text) : wxDataViewVirtualListModel(0), text_(text) {}
    void GetValueByRow(wxVariant& variant, unsigned row, unsigned column) const override {
        variant = (*text_)(static_cast<long>(row), static_cast<int>(column));
    }
    bool SetValueByRow(const wxVariant&, unsigned, unsigned) override { return false; }

private:
    TableView::TextFn* text_;
};
}  // namespace

struct TableView::Impl {
    wxDataViewCtrl* view = nullptr;
    VirtualModel* model = nullptr;
};

TableView::TableView(wxWindow* parent, const std::vector<Column>& columns, bool multiSelect, TextFn text,
                     const wxString& accessibleName)
    : wxPanel(parent), impl_(new Impl), text_(std::move(text)) {
    impl_->view = new wxDataViewCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                     (multiSelect ? wxDV_MULTIPLE : wxDV_SINGLE) | wxDV_ROW_LINES);
    impl_->model = new VirtualModel(&text_);
    impl_->view->AssociateModel(impl_->model);
    impl_->model->DecRef();  // the control holds the reference
    unsigned column = 0;
    for (const Column& c : columns)
        impl_->view->AppendTextColumn(c.title, column++, wxDATAVIEW_CELL_INERT, FromDIP(c.width), wxALIGN_LEFT,
                                      wxDATAVIEW_COL_RESIZABLE);
    ctrl_ = impl_->view;
    impl_->view->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, [this](wxDataViewEvent&) {
        if (onActivated) onActivated();
    });
#endif

    // Common to both
    a11y::setName(ctrl_, accessibleName);
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(ctrl_, 1, wxEXPAND);
    SetSizer(sizer);
    // wxEVT_CHAR_HOOK goes from the focused control up through its parents before the control handles the key
    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& e) {
        if (onKey && onKey(e)) return;
        e.Skip();
    });
}

TableView::~TableView() { delete impl_; }

#if KMIDI_TABLE_LISTCTRL

void TableView::setRowCount(long rows) {
    rows_ = rows;
    impl_->list->SetItemCount(rows);
    impl_->list->Refresh();
}

void TableView::refreshAll() { impl_->list->Refresh(); }

std::vector<long> TableView::selectedRows() const {
    std::vector<long> rows;
    long item = -1;
    while ((item = impl_->list->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) != -1) rows.push_back(item);
    return rows;
}

void TableView::selectRow(long row) {
    if (row < 0 || row >= rows_) return;
    for (long r : selectedRows())
        if (r != row) impl_->list->SetItemState(r, 0, wxLIST_STATE_SELECTED);
    impl_->list->SetItemState(row, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                              wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
    impl_->list->EnsureVisible(row);
}

#else

void TableView::setRowCount(long rows) {
    std::vector<long> selection = selectedRows();
    rows_ = rows;
    impl_->model->Reset(static_cast<unsigned>(rows));
    wxDataViewItemArray items;
    for (long r : selection)
        if (r < rows) items.Add(impl_->model->GetItem(static_cast<unsigned>(r)));
    impl_->view->SetSelections(items);
}

void TableView::refreshAll() {
    // Changing every row (rather than resetting the model) keeps VoiceOver's place in the table
    for (long r = 0; r < rows_; ++r) impl_->model->RowChanged(static_cast<unsigned>(r));
}

std::vector<long> TableView::selectedRows() const {
    std::vector<long> rows;
    wxDataViewItemArray items;
    impl_->view->GetSelections(items);
    for (const auto& item : items) rows.push_back(static_cast<long>(impl_->model->GetRow(item)));
    std::sort(rows.begin(), rows.end());
    return rows;
}

void TableView::selectRow(long row) {
    if (row < 0 || row >= rows_) return;
    wxDataViewItem item = impl_->model->GetItem(static_cast<unsigned>(row));
    impl_->view->UnselectAll();
    impl_->view->Select(item);
    impl_->view->SetCurrentItem(item);
    impl_->view->EnsureVisible(item);
}

#endif

long TableView::selectedRow() const {
    auto rows = selectedRows();
    return rows.empty() ? -1 : rows.front();
}
