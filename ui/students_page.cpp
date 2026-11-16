#include "students_page.h"

#include "dialogs.h"
#include <QShortcut>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileInfo>
#include <QListWidget>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>

namespace {
class NumericTableItem : public QTableWidgetItem {
public:
    using QTableWidgetItem::QTableWidgetItem;
    bool operator<(const QTableWidgetItem &other) const override {
        if (column() == 3)
            return data(Qt::UserRole).toDouble() < other.data(Qt::UserRole).toDouble();
        return QTableWidgetItem::operator<(other);
    }
};

QWidget *undoBanner(QWidget *parent, const QString &message, std::function<void()> undo) {
    auto *banner = new QFrame(parent);
    banner->setObjectName("toast");
    auto *layout = new QHBoxLayout(banner);
    layout->setContentsMargins(14, 8, 10, 8);
    layout->addWidget(new QLabel(message, banner));
    auto *button = new QPushButton("Undo", banner);
    layout->addWidget(button);
    QObject::connect(button, &QPushButton::clicked, banner, [banner, undo]() {
        undo();
        banner->deleteLater();
    });
    banner->adjustSize();
    banner->move(qMax(12, parent->width() - banner->width() - 24),
                 qMax(12, parent->height() - banner->height() - 24));
    banner->show();
    banner->raise();
    QTimer::singleShot(6000, banner, &QObject::deleteLater);
    return banner;
}

void openDocumentVault(const Ctx &ctx, const QString &email, QWidget *parent) {
    QDialog dialog(parent);
    dialog.setWindowTitle("Student document vault");
    dialog.resize(600, 420);
    auto *layout = new QVBoxLayout(&dialog);
    auto *list = new QListWidget(&dialog);
    const QVector<VaultDocument> documents = ctx.store->studentDocuments(email);
    for (const VaultDocument &document : documents) {
        auto *item = new QListWidgetItem(
            document.name + (document.verified ? " · Verified" : " · Pending verification"), list);
        item->setData(Qt::UserRole, document.id);
    }
    auto *empty = Theme::emptyState("No documents uploaded yet",
                                    "Documents added by this student will appear here.");
    empty->setVisible(documents.isEmpty());
    list->setVisible(!documents.isEmpty());
    auto *preview = Theme::button("Preview", "soft");
    auto *verify = Theme::button("Verify selected", "primary");
    auto *close = Theme::button("Close", "ghost");
    preview->setEnabled(!documents.isEmpty());
    verify->setEnabled(ctx.store->canVerifyStudentDocuments() && !documents.isEmpty());
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(preview);
    buttons->addWidget(verify);
    buttons->addStretch();
    buttons->addWidget(close);
    layout->addWidget(empty, 1);
    layout->addWidget(list, 1);
    layout->addLayout(buttons);
    QObject::connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    QObject::connect(preview, &QPushButton::clicked, &dialog, [&]() {
        QListWidgetItem *item = list->currentItem();
        if (!item) return;
        const qint64 id = item->data(Qt::UserRole).toLongLong();
        for (const VaultDocument &document : documents) {
            if (document.id != id) continue;
            const QString folder = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                + "/PlaceBridgeVault";
            if (!QDir().mkpath(folder)) {
                QMessageBox::critical(&dialog, "Preview failed", "Could not create a temporary folder.");
                return;
            }
            const QString path = folder + "/" + QFileInfo(document.name).fileName();
            QSaveFile file(path);
            if (!file.open(QIODevice::WriteOnly) || file.write(document.content) != document.content.size()
                || !file.commit()) {
                QMessageBox::critical(&dialog, "Preview failed", file.errorString());
                return;
            }
            if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
                QMessageBox::warning(&dialog, "Preview failed", "No application can open this document.");
            return;
        }
    });
    QObject::connect(verify, &QPushButton::clicked, &dialog, [&]() {
        QListWidgetItem *item = list->currentItem();
        if (!item) return;
        const qint64 id = item->data(Qt::UserRole).toLongLong();
        if (!ctx.store->verifyDocument(id)) {
            QMessageBox::warning(&dialog, "Verification failed", "Could not verify this document.");
            return;
        }
        for (const VaultDocument &document : documents)
            if (document.id == id) {
                item->setText(document.name + " · Verified");
                break;
            }
    });
    dialog.exec();
}
}

StudentsPage::StudentsPage(const Ctx &context) : ctx(context) {
    const bool admin = ctx.store->adminActor();
    auto *importButton = Theme::button("Import Student CSV", "ghost");
    auto *addButton = Theme::button("+ Add New Candidate", "primary");
    importButton->setVisible(admin);
    addButton->setVisible(admin);
    connect(importButton, &QPushButton::clicked, this, [this]() { importCsv(); });
    connect(addButton, &QPushButton::clicked, this, [this]() {
        StudentDialog dialog(ctx.store, this);
        if (dialog.exec() == QDialog::Accepted) {
            if (ctx.store->addStudent(dialog.student())) Theme::toast(this, "Candidate added");
            else QMessageBox::warning(this, "Could not add candidate",
                                      "The e-mail or roll number already exists, or permission was denied.");
        }
    });
    lay->addWidget(Theme::hero("Class of 2026 Candidate Registry", "Student Candidates",
                               "Search, sort and manage the placement registry.",
                               { addButton, importButton }));

    auto *bar = new Theme::ResponsiveGrid(180, 8);
    auto *branches = new QComboBox;
    branches->addItem("All Branches");
    QSet<QString> branchNames;
    for (const Student &student : ctx.store->students()) branchNames.insert(student.branch());
    QStringList sortedBranches = branchNames.values();
    sortedBranches.sort();
    branches->addItems(sortedBranches);
    connect(branches, &QComboBox::currentTextChanged, this, [this](const QString &value) {
        branch = value;
        currentPage = 0;
        refresh();
    });
    branches->setMinimumWidth(0);
    branches->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    bar->addWidget(branches);
    search = new QLineEdit;
    search->setPlaceholderText("Search name, e-mail, roll number");
    search->setMinimumWidth(0);
    search->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(search, &QLineEdit::textChanged, this, [this]() { currentPage = 0; refresh(); });
    bar->addWidget(search);
    auto *selectAll = new QCheckBox("Select page", this);
    connect(selectAll, &QCheckBox::toggled, this, [this](bool checked) {
        if (!table) return;
        for (int row = 0; row < table->rowCount(); ++row)
            if (auto *item = table->item(row, 0)) item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    });
    selectAll->setVisible(admin);
    selectAll->setMinimumHeight(40);
    if (admin) bar->addWidget(selectAll);
    auto *shortlist = Theme::button("Shortlist selected", "soft");
    auto *remove = Theme::button("Delete selected", "danger");
    auto *exportButton = Theme::button("Export selected CSV", "soft");
    shortlist->setParent(this);
    remove->setParent(this);
    exportButton->setParent(this);
    connect(shortlist, &QPushButton::clicked, this, [this]() { shortlistSelected(); });
    connect(remove, &QPushButton::clicked, this, [this]() { removeSelected(); });
    connect(exportButton, &QPushButton::clicked, this, [this]() { exportSelected(); });
    shortlist->setMinimumHeight(40);
    remove->setMinimumHeight(40);
    exportButton->setMinimumHeight(40);
    shortlist->setVisible(admin);
    remove->setVisible(admin);
    exportButton->setVisible(admin);
    if (admin) {
        bar->addWidget(shortlist);
        bar->addWidget(remove);
        bar->addWidget(exportButton);
    }
    lay->addWidget(bar);

    table = Theme::table({ "SELECT", "CANDIDATE", "BRANCH", "CGPA", "STATUS", "ROLL NO.", "PROFILE", "DOCUMENTS" });
    // Qt's built-in sort moves items but leaves setCellWidget() widgets in their
    // original rows, so the Verify / Documents buttons ended up next to the wrong
    // candidate. Sorting is therefore done over the model in refresh() and the
    // header only reports which column was clicked.
    table->setSortingEnabled(false);
    table->horizontalHeader()->setSectionsClickable(true);
    table->horizontalHeader()->setSortIndicatorShown(true);
    table->horizontalHeader()->setSortIndicator(sortColumn, sortOrder);
    connect(table->horizontalHeader(), &QHeaderView::sectionClicked, this, [this](int column) {
        if (column == 0 || column > 5) return;             // checkbox and action columns
        if (column == sortColumn)
            sortOrder = sortOrder == Qt::AscendingOrder ? Qt::DescendingOrder : Qt::AscendingOrder;
        else
            sortOrder = Qt::AscendingOrder;
        sortColumn = column;
        currentPage = 0;
        refresh();
    });
    lay->addWidget(Theme::tableCard("Candidate registry",
                                   "Click a column heading to sort. CGPA sorts numerically.", nullptr, table));
    emptyState = Theme::emptyState("No candidates found",
        "Try another search or branch, or clear your filters.");
    emptyState->hide();
    lay->addWidget(emptyState);

    auto *paging = new Theme::ResponsiveGrid(140, 8);
    previous = Theme::button("Previous", "soft");
    next = Theme::button("Next", "soft");
    pageLabel = Theme::label("", "muted");
    pageSize = new QComboBox;
    pageSize->addItems({ "25", "50", "100" });
    connect(previous, &QPushButton::clicked, this, [this]() { currentPage = qMax(0, currentPage - 1); refresh(); });
    connect(next, &QPushButton::clicked, this, [this]() { ++currentPage; refresh(); });
    connect(pageSize, &QComboBox::currentTextChanged, this, [this]() { currentPage = 0; refresh(); });
    previous->setMinimumHeight(40);
    next->setMinimumHeight(40);
    pageSize->setMinimumWidth(0);
    paging->addWidget(previous);
    paging->addWidget(pageLabel);
    paging->addWidget(next);
    paging->addWidget(Theme::label("Rows per page", "muted"));
    paging->addWidget(pageSize);
    lay->addWidget(paging);
    lay->addStretch();

    auto *addShortcut = new QShortcut(QKeySequence::New, this);
    connect(addShortcut, &QShortcut::activated, addButton, &QPushButton::click);
    auto *findShortcut = new QShortcut(QKeySequence::Find, this);
    connect(findShortcut, &QShortcut::activated, search, qOverload<>(&QWidget::setFocus));
    refresh();
}

void StudentsPage::refresh() {
    if (!table) return;
    const bool admin = ctx.store->adminActor();
    QVector<Student> rows;
    const QString query = search->text().trimmed().toCaseFolded();
    for (const Student &student : ctx.store->students()) {
        if (branch != "All Branches" && student.branch() != branch) continue;
        if (!query.isEmpty() &&
            !(student.getName() + " " + student.getEmail() + " " + student.getRollNo())
                 .toCaseFolded().contains(query)) continue;
        rows.append(student);
    }
    auto compare = [this](const Student &left, const Student &right) {
        int cmp = 0;
        switch (sortColumn) {
        case 1: cmp = QString::compare(left.getName(), right.getName(), Qt::CaseInsensitive); break;
        case 2: cmp = QString::compare(left.branch(), right.branch(), Qt::CaseInsensitive); break;
        case 3: cmp = left.cgpaValue() < right.cgpaValue() ? -1 : left.cgpaValue() > right.cgpaValue() ? 1 : 0; break;
        case 4: cmp = QString::compare(left.getStatus(), right.getStatus(), Qt::CaseInsensitive); break;
        case 5: cmp = QString::compare(left.getRollNo(), right.getRollNo(), Qt::CaseInsensitive); break;
        default: cmp = QString::compare(left.getName(), right.getName(), Qt::CaseInsensitive);
        }
        return sortOrder == Qt::AscendingOrder ? cmp < 0 : cmp > 0;
    };
    std::stable_sort(rows.begin(), rows.end(), compare);
    const int size = pageSize->currentText().toInt();
    const int pageCount = qMax(1, (rows.size() + size - 1) / size);
    currentPage = qBound(0, currentPage, pageCount - 1);
    const int first = currentPage * size;
    displayed.clear();
    for (int i = first; i < qMin(first + size, rows.size()); ++i) displayed.append(rows[i]);

    table->setRowCount(displayed.size());
    for (int row = 0; row < displayed.size(); ++row) {
        const Student student = displayed[row];
        auto *check = new QTableWidgetItem;
        check->setFlags(admin ? Qt::ItemIsEnabled | Qt::ItemIsUserCheckable : Qt::NoItemFlags);
        check->setCheckState(Qt::Unchecked);
        check->setData(Qt::UserRole, student.getEmail());
        table->setItem(row, 0, check);
        Theme::setText(table, row, 1, student.getName(), true);
        table->item(row, 1)->setData(Qt::UserRole, student.getEmail());
        Theme::setText(table, row, 2, student.branch());
        auto *cgpaItem = new NumericTableItem(student.getCgpa());
        bool ok = false;
        const double cgpa = student.getCgpa().toDouble(&ok);
        cgpaItem->setData(Qt::UserRole, ok ? cgpa : -1.0);
        table->setItem(row, 3, cgpaItem);
        Theme::setText(table, row, 4, student.getStatus());
        Theme::setText(table, row, 5, student.getRollNo());
        auto *verification = Theme::button(student.isVerified() ? "Verified" : "Verify",
                                            student.isVerified() ? "done" : "soft");
        verification->setVisible(admin);
        verification->setEnabled(!student.isVerified());
        connect(verification, &QPushButton::clicked, this, [this, student]() {
            if (ctx.store->verifyStudent(student.getEmail()))
                Theme::toast(this, student.getName() + " profile verified");
        });
        table->setCellWidget(row, 6, verification);
        auto *documents = Theme::button("Documents", "soft");
        documents->setVisible(admin);
        connect(documents, &QPushButton::clicked, this, [this, student]() {
            openDocumentVault(ctx, student.getEmail(), this);
        });
        table->setCellWidget(row, 7, documents);
    }
    table->horizontalHeader()->setSortIndicator(sortColumn, sortOrder);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    table->setMinimumHeight(qMin(600, 42 + displayed.size() * 42));
    table->setMaximumHeight(qMin(600, 42 + displayed.size() * 42));
    emptyState->setVisible(rows.isEmpty());
    pageLabel->setText(rows.isEmpty() ? "0 candidates"
        : QString("Page %1 of %2 · %3 students").arg(currentPage + 1).arg(pageCount).arg(rows.size()));
    previous->setEnabled(currentPage > 0);
    next->setEnabled(currentPage + 1 < pageCount);
}

QVector<Student> StudentsPage::selectedStudents() const {
    // Rows and `displayed` are built together in refresh(), so row index is the
    // lookup; the old nested scan was quadratic in the page size.
    QVector<Student> selected;
    for (int row = 0; row < table->rowCount() && row < displayed.size(); ++row) {
        const auto *check = table->item(row, 0);
        if (!check || check->checkState() != Qt::Checked) continue;
        selected.append(displayed[row]);
    }
    return selected;
}

void StudentsPage::shortlistSelected() {
    const auto selected = selectedStudents();
    if (selected.isEmpty()) return Theme::toast(this, "Select at least one candidate");
    int changed = 0;
    for (const Student &student : selected)
        if (ctx.store->setStudentStatus(student.getEmail(), "Shortlisted")) ++changed;
    Theme::toast(this, QString("%1 candidate(s) shortlisted").arg(changed));
}

void StudentsPage::removeSelected() {
    const auto selected = selectedStudents();
    if (selected.isEmpty()) return Theme::toast(this, "Select at least one candidate");
    if (QMessageBox::question(this, "Delete candidates",
            QString("Delete %1 selected candidate(s)?").arg(selected.size())) != QMessageBox::Yes) return;
    QVector<Student> deleted;
    for (const Student &student : selected)
        if (ctx.store->removeStudent(student.getEmail())) deleted.append(student);
    if (deleted.isEmpty()) return;
    QPointer<StudentsPage> page(this);
    DataStore *store = ctx.store;
    undoBanner(window(), QString("%1 candidate(s) deleted").arg(deleted.size()), [page, store, deleted]() {
        int restored = 0;
        for (const Student &student : deleted)
            if (store->restoreStudent(student)) ++restored;
        if (page) Theme::toast(page, QString("%1 candidate(s) restored").arg(restored));
    });
}

void StudentsPage::exportSelected() {
    const auto selected = selectedStudents();
    if (selected.isEmpty()) return Theme::toast(this, "Select at least one candidate");
    QString csv = Theme::csvLine({ "Name", "Email", "Branch", "CGPA", "Status", "Roll No" });
    for (const Student &student : selected)
        csv += Theme::csvLine({ student.getName(), student.getEmail(), student.branch(),
                                student.getCgpa(), student.getStatus(), student.getRollNo() });
    Theme::saveTextFile(this, "Export selected candidates", "students.csv", "CSV (*.csv)", csv);
}

void StudentsPage::importCsv() {
    const QString path = QFileDialog::getOpenFileName(this, "Import students", QDir::homePath(),
                                                       "CSV files (*.csv *.txt)");
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Import failed", "Could not open:\n" + path);
        return;
    }
    QTextStream in(&file);
    QVector<Student> students;
    int skipped = 0;
    while (!in.atEnd()) {
        const QStringList p = in.readLine().split(',');
        if (p.size() < 3 || p[1].trimmed().count('@') != 1) { ++skipped; continue; }
        students.append(Student(p[0].trimmed(), p[1].trimmed(), p[2].trimmed(),
                                p.size() > 3 && !p[3].trimmed().isEmpty() ? p[3].trimmed() : "N/A",
                                p.size() > 4 && !p[4].trimmed().isEmpty() ? p[4].trimmed() : "Registered",
                                p.size() > 5 ? p[5].trimmed() : QString()));
    }
    const int added = ctx.store->addStudents(students);
    QMessageBox::information(this, "Import finished",
        QString("%1 student(s) added.\n%2 row(s) skipped.").arg(added).arg(skipped + students.size() - added));
}
