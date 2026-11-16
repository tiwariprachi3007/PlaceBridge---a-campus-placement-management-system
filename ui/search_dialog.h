#ifndef SEARCH_DIALOG_H
#define SEARCH_DIALOG_H

#include "theme.h"
#include "context.h"
#include "SystemServices.h"

// Ctrl+K palette: type to filter across students, drives, applications and events;
// double-click (or Enter) jumps to the tab that shows the record.
class SearchDialog : public QDialog {
    DataStore *store;
    QLineEdit *input;
    QListWidget *list;
    QLabel *status;
    QVector<SearchHit> hits;
    QString chosenTab;
    QTimer *debounce = nullptr;

public:
    explicit SearchDialog(DataStore *dataStore, QWidget *parent = nullptr)
        : QDialog(parent), store(dataStore) {
        setWindowTitle("Search PlaceBridge");
        setModal(true);
        setMinimumSize(520, 420);
        auto *v = new QVBoxLayout(this);
        v->setContentsMargins(24, 20, 24, 18);
        v->setSpacing(10);
        v->addWidget(Theme::label("Search PlaceBridge", "h2"));
        input = new QLineEdit;
        input->setPlaceholderText("Student, company, role, e-mail, event…");
        input->setClearButtonEnabled(true);
        input->setAccessibleName("Search query");
        v->addWidget(input);
        status = Theme::label("Type to search the records you can access.", "muted", nullptr, true);
        v->addWidget(status);
        list = new QListWidget;
        list->setAccessibleName("Search results");
        v->addWidget(list, 1);

        // Every keystroke used to re-run four table scans; coalesce them.
        debounce = new QTimer(this);
        debounce->setSingleShot(true);
        debounce->setInterval(140);
        connect(debounce, &QTimer::timeout, this, [this]() { refresh(); });
        connect(input, &QLineEdit::textChanged, this, [this]() { debounce->start(); });
        connect(input, &QLineEdit::returnPressed, this, [this]() {
            if (list->count() > 0) choose(list->currentRow() >= 0 ? list->currentRow() : 0);
        });
        connect(list, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
            choose(list->row(item));
        });
        input->setFocus();
    }

    // Tab to open after the dialog was accepted (empty when nothing was chosen).
    QString selectedTab() const { return chosenTab; }

private:
    void refresh() {
        list->clear();
        hits = GlobalSearch::run(*store, input->text());
        if (input->text().trimmed().isEmpty()) {
            status->setText("Type to search the records you can access.");
            return;
        }
        status->setText(hits.isEmpty() ? "No matching records."
                                       : QString("%1 result(s)").arg(hits.size()));
        for (const SearchHit &hit : hits)
            list->addItem(QString("[%1]  %2\n      %3").arg(hit.kind, hit.title, hit.detail));
        if (!hits.isEmpty()) list->setCurrentRow(0);
    }

    void choose(int row) {
        if (row < 0 || row >= hits.size()) return;
        chosenTab = hits[row].tab;
        accept();
    }
};

#endif // SEARCH_DIALOG_H
