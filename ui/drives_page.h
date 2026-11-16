#ifndef DRIVES_PAGE_H
#define DRIVES_PAGE_H

#include "theme.h"
#include "context.h"
#include "dialogs.h"
#include "placement_officer.h"
#include <QSet>
#include <algorithm>

// ============================================================================
// Recruitment drives. Same page for everybody, but the card action depends on
// who is signed in (runtime type check on the polymorphic User):
//   PlacementOfficer -> Manage Drive      Student -> Quick Apply
// ============================================================================
class DrivesPage : public Theme::Page {
    Ctx ctx;
    bool isAdmin = false;
    std::optional<Student> student;       // set when a student is signed in
    QString filter = "All";
    QLineEdit *search = nullptr;
    QComboBox *sortBox = nullptr;
    QTimer *debounce = nullptr;
    QGridLayout *controlsLayout = nullptr;
    QVector<QPushButton *> filterButtons;
    QWidget *gridHost = nullptr;
    QGridLayout *grid = nullptr;
    int colCount = 3;
    QHash<QString, int> applicantCounts;   // drive key -> applications
    QSet<QString> appliedKeys;             // drives this student already applied to
    QHash<QString, QString> eligibility;   // drive key -> blocking reason ("" = eligible)

protected:
    void resizeEvent(QResizeEvent *e) override {
        Theme::Page::resizeEvent(e);
        reflowControls();
        if (gridHost && Theme::columnsFor(width()) != colCount)
            QTimer::singleShot(0, this, [this]() { refresh(); });
    }


public:
    explicit DrivesPage(const Ctx &c) : ctx(c) {
        isAdmin = dynamic_cast<PlacementOfficer *>(ctx.user) != nullptr;
        if (auto *s = dynamic_cast<Student *>(ctx.user)) student = ctx.store->findStudent(s->getEmail());

        auto *exportBtn = Theme::button("Export Schedule", "ghost");
        QObject::connect(exportBtn, &QPushButton::clicked, this, [this]() { exportCsv(); });
        QList<QPushButton *> actions = { exportBtn };
        if (isAdmin) {
            auto *add = Theme::button("+ Schedule New Drive", "primary");
            QObject::connect(add, &QPushButton::clicked, this, [this]() {
                DriveDialog dlg(QString(), this);
                if (dlg.exec() == QDialog::Accepted) { ctx.store->addDrive(dlg.drive()); Theme::toast(this, "Drive scheduled"); }
            });
            actions.prepend(add);
        }
        lay->addWidget(Theme::hero("Corporate Recruitment Drives", "Campus Recruitment Drives",
                                   isAdmin ? "Browse, configure, and manage active and upcoming company selection schedules for batch 2026."
                                           : "Browse open drives and apply when you meet the CGPA cutoff.",
                                   actions));

        // chips + search
        auto *barHost = new QWidget;
        controlsLayout = new QGridLayout(barHost);
        controlsLayout->setContentsMargins(0, 0, 0, 0);
        controlsLayout->setHorizontalSpacing(8);
        controlsLayout->setVerticalSpacing(8);
        auto *group = new QButtonGroup(this);
        group->setExclusive(true);
        for (const QString &f : QStringList{ "All", "Open", "Scheduled", "Completed" }) {
            auto *chip = Theme::button(f, "chip");
            chip->setMinimumHeight(40);
            chip->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            chip->setChecked(f == "All");
            group->addButton(chip);
            filterButtons.append(chip);
            QObject::connect(chip, &QPushButton::clicked, this, [this, f]() { filter = f; refresh(); });
        }
        sortBox = new QComboBox;
        sortBox->addItems({ "Sort: Default", "Sort: Package (high to low)", "Sort: Company A-Z" });
        sortBox->setAccessibleName("Sort drives");
        sortBox->setToolTip("Change the order of the drive cards");
        sortBox->setMinimumWidth(0);
        sortBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        QObject::connect(sortBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                         [this]() { refresh(); });
        search = new QLineEdit;
        search->setPlaceholderText("Search company or role…");
        search->setMinimumWidth(0);
        search->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        debounce = new QTimer(this);
        debounce->setSingleShot(true);
        debounce->setInterval(160);
        QObject::connect(debounce, &QTimer::timeout, this, [this]() { refresh(); });
        QObject::connect(search, &QLineEdit::textChanged, this, [this]() { debounce->start(); });
        lay->addWidget(barHost);
        reflowControls();

        gridHost = new QWidget;
        grid = new QGridLayout(gridHost);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(18);
        for (int c2 = 0; c2 < 3; ++c2) grid->setColumnStretch(c2, 1);
        lay->addWidget(gridHost);
        lay->addStretch();
        refresh();
    }

private:
    void reflowControls() {
        if (!controlsLayout || filterButtons.isEmpty() || !sortBox || !search) return;
        while (QLayoutItem *item = controlsLayout->takeAt(0)) delete item;
        for (int i = 0; i < 6; ++i) controlsLayout->setColumnStretch(i, 0);
        for (int i = 0; i < 4; ++i) controlsLayout->setRowStretch(i, 0);
        if (width() < 980) {
            for (int i = 0; i < filterButtons.size(); ++i)
                controlsLayout->addWidget(filterButtons[i], i / 2, i % 2);
            controlsLayout->addWidget(sortBox, 2, 0, 1, 2);
            controlsLayout->addWidget(search, 3, 0, 1, 2);
            controlsLayout->setColumnStretch(0, 1);
            controlsLayout->setColumnStretch(1, 1);
            controlsLayout->setRowStretch(3, 1);
        } else {
            for (int i = 0; i < filterButtons.size(); ++i)
                controlsLayout->addWidget(filterButtons[i], 0, i);
            controlsLayout->addWidget(sortBox, 0, 4);
            controlsLayout->addWidget(search, 0, 5);
            controlsLayout->setColumnStretch(5, 1);
        }
    }

    void refresh() {
        gridHost->setUpdatesEnabled(false);
        Theme::clearLayout(grid);
        colCount = Theme::columnsFor(width());
        auto drives = ctx.store->drives();
        const int sortMode = sortBox ? sortBox->currentIndex() : 0;
        if (sortMode == 1)
            std::stable_sort(drives.begin(), drives.end(),
                             [](const Company &a, const Company &b) { return a.lpa() > b.lpa(); });
        else if (sortMode == 2)
            std::stable_sort(drives.begin(), drives.end(), [](const Company &a, const Company &b) {
                return a.getName().compare(b.getName(), Qt::CaseInsensitive) < 0; });

        // Everything each card needs is gathered once here. Previously every card
        // ran its own hasApplied() and eligibilityForDrive() query, and the latter
        // re-read the whole drive table, so a page of N drives cost O(N^2) queries.
        applicantCounts.clear();
        const auto apps = ctx.store->applications();
        for (const Application &a : apps) ++applicantCounts[a.driveKey()];
        if (student) {
            appliedKeys = ctx.store->appliedDriveKeys(student->getName());
            eligibility = ctx.store->eligibilityForVisibleDrives(student->getEmail());
        }

        const QString q = search ? search->text().trimmed().toLower() : QString();
        int n = 0;
        for (const Company &d : drives) {
            if (filter != "All" && d.getDriveStatus() != filter) continue;
            if (!q.isEmpty() && !(d.getName() + " " + d.getJobRole()).toLower().contains(q)) continue;
            grid->addWidget(driveCard(d, applicantCounts.value(
                                Application::driveKey(d.getName(), d.getJobRole()))),
                            n / colCount, n % colCount);
            ++n;
        }
        if (n == 0)
            grid->addWidget(Theme::emptyState("No drives match this filter",
                                              "Clear the search box or pick another status chip."),
                            0, 0, 1, colCount);
        for (int r = 0; r <= n / colCount; ++r) grid->setRowStretch(r, 0);
        grid->setRowStretch(n / colCount + 1, 1);
        gridHost->setUpdatesEnabled(true);
    }

    QFrame *driveCard(const Company &d, int applicants) {
        auto *c = Theme::card();
        auto *v = Theme::vbox(c, 22, 14);
        auto *top = new QHBoxLayout;
        top->setSpacing(12);
        top->addWidget(Theme::logo(d.initials(), d.lpa() >= 20));
        auto *names = new QVBoxLayout;
        names->setSpacing(0);
        names->addWidget(Theme::label(d.getName(), "h2"));
        names->addWidget(Theme::label(d.getJobRole(), "muted", nullptr, true));
        top->addLayout(names, 1);
        top->addWidget(Theme::statusPill(d.getDriveStatus()), 0, Qt::AlignTop);
        v->addLayout(top);
        v->addWidget(Theme::divider());

        auto *g = new QGridLayout;
        g->setHorizontalSpacing(20);
        g->setVerticalSpacing(10);
        auto fact = [&](int r, int col, const QString &k, const QString &val) {
            auto *b = new QVBoxLayout;
            b->setSpacing(2);
            b->addWidget(Theme::label(k, "eyebrow"));
            b->addWidget(Theme::label(val, "value"));
            g->addLayout(b, r, col);
        };
        fact(0, 0, "CTC PACKAGE", Fmt::lpa(d.lpa()));
        fact(0, 1, "DRIVE DATE", Fmt::dateLong(d.date()));
        fact(1, 0, "MIN CGPA", d.getMinCgpa().isEmpty() ? "None" : d.getMinCgpa() + " & above");
        fact(1, 1, "APPLICANTS", QString::number(applicants) + (d.getDriveStatus() == "Completed" ? " Candidates" : " Applied"));
        v->addLayout(g);
        if (!d.getJobDescription().isEmpty())
            v->addWidget(Theme::label(d.getJobDescription(), "muted", nullptr, true));
        if (!d.getEligibility().isEmpty())
            v->addWidget(Theme::label("Eligibility: " + d.getEligibility(), "muted", nullptr, true));
        if (!d.getRounds().isEmpty())
            v->addWidget(Theme::label("Rounds: " + d.getRounds(), "muted", nullptr, true));
        v->addSpacing(2);
        v->addWidget(actionFor(d));
        return c;
    }

    QPushButton *actionFor(const Company &d) {
        if (isAdmin) {
            if (!d.isApproved()) {
                auto *approve = Theme::button("Approve Drive", "primary");
                QObject::connect(approve, &QPushButton::clicked, this, [this, d]() {
                    if (ctx.store->approveDrive(d.getName(), d.getJobRole()))
                        Theme::toast(this, "Drive approved and students notified");
                });
                return approve;
            }
            auto *b = Theme::button(d.getDriveStatus() == "Completed" ? "Review Drive ➔" : "Manage Drive ➔", "soft");
            QObject::connect(b, &QPushButton::clicked, this, [this, d]() {
                DriveManageDialog dlg(ctx.store, d, this);
                dlg.exec();
            });
            return b;
        }
        if (student) {
            const Student s = *student;
            const QString key = Application::driveKey(d.getName(), d.getJobRole());
            if (appliedKeys.contains(key)) {
                auto *b = Theme::button("Applied ✓", "done");
                b->setEnabled(false);
                return b;
            }
            if (d.getDriveStatus() != "Open") {
                auto *b = Theme::button("Not accepting applications", "soft");
                b->setEnabled(false);
                return b;
            }
            const QString reasonText = eligibility.value(key);
            if (!reasonText.isEmpty()) {
                auto *b = Theme::button("Not eligible", "soft");
                b->setEnabled(false);
                b->setToolTip(reasonText);
                return b;
            }
            auto *b = Theme::button("Quick Apply ➔", "primary");
            QObject::connect(b, &QPushButton::clicked, this, [this, s, d]() {
                if (ctx.store->addApplication({ s.getName(), d.getName(), d.getJobRole(), "Applied" }))
                    Theme::toast(this, QString("Application submitted to %1").arg(d.getName()));
            });
            return b;
        }
        auto *b = Theme::button("View Details", "soft");
        b->setEnabled(false);
        return b;
    }

    void exportCsv() {
        QString out = Theme::csvLine({ "Company", "Role", "Package", "Status", "Date", "Min CGPA" });
        for (const Company &d : ctx.store->drives())
            out += Theme::csvLine({ d.getName(), d.getJobRole(), d.getPackageOffer(), d.getDriveStatus(), d.getDriveDate(), d.getMinCgpa() });
        Theme::saveTextFile(this, "Export drive schedule", "drive_schedule.csv", "CSV files (*.csv)", out);
    }
};

// ============================================================================
// Visiting employers directory (admin)
// ============================================================================
class CompaniesPage : public Theme::Page {
    struct Summary { QString name; int drives = 0, offers = 0; double top = 0; QDate next; };

    Ctx ctx;
    QString sector = "All sectors";
    QWidget *gridHost = nullptr;
    QGridLayout *grid = nullptr;
    int colCount = 3;
    QVector<Summary> cachedSummaries;   // computed once; a data change rebuilds the page

protected:
    void resizeEvent(QResizeEvent *e) override {
        Theme::Page::resizeEvent(e);
        if (gridHost && Theme::columnsFor(width()) != colCount)
            QTimer::singleShot(0, this, [this]() { refresh(); });
    }


public:
    explicit CompaniesPage(const Ctx &c) : ctx(c) {
        auto *add = Theme::button("+ Add New Partner", "primary");
        auto *exp = Theme::button("Export Directory", "ghost");
        QObject::connect(add, &QPushButton::clicked, this, [this]() {
            DriveDialog dlg(QString(), this);
            if (dlg.exec() == QDialog::Accepted) { ctx.store->addDrive(dlg.drive()); Theme::toast(this, "Drive scheduled"); }
        });
        QObject::connect(exp, &QPushButton::clicked, this, [this]() { exportCsv(); });

        QSet<QString> names;
        for (const Company &d : ctx.store->drives()) names.insert(d.getName());
        lay->addWidget(Theme::hero("Corporate Recruitment Partners", "Visiting Employers",
                                   QString("%1 recruiting partners with their drives, offers and verified CTC benchmarks.").arg(names.size()),
                                   { add, exp }));

        auto *bar = new Theme::ResponsiveGrid(135, 8);
        auto *group = new QButtonGroup(this);
        for (const QString &s : QStringList{ "All sectors", "Product", "IT services", "Consulting", "Core" }) {
            auto *chip = Theme::button(s, "chip");
            chip->setMinimumHeight(40);
            chip->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            chip->setChecked(s == "All sectors");
            group->addButton(chip);
            bar->addWidget(chip);
            QObject::connect(chip, &QPushButton::clicked, this, [this, s]() { sector = s; refresh(); });
        }
        lay->addWidget(bar);

        gridHost = new QWidget;
        grid = new QGridLayout(gridHost);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(18);
        for (int i = 0; i < 3; ++i) grid->setColumnStretch(i, 1);
        lay->addWidget(gridHost);
        lay->addStretch();
        cachedSummaries = summaries();
        refresh();
    }

private:

    // Grouped with hash lookups rather than a linear scan per row, so this is
    // linear in drives + applications instead of quadratic.
    QVector<Summary> summaries() const {
        QVector<Summary> out;
        QHash<QString, int> indexByName;
        QHash<QString, int> offersByCompany;
        for (const Application &a : ctx.store->applications())
            if (a.stage() >= 3) ++offersByCompany[a.company];
        for (const Company &d : ctx.store->drives()) {
            auto it = indexByName.constFind(d.getName());
            if (it == indexByName.constEnd()) {
                Summary fresh;
                fresh.name = d.getName();
                out.append(fresh);
                it = indexByName.insert(d.getName(), out.size() - 1);
            }
            Summary &s = out[it.value()];
            ++s.drives;
            s.top = qMax(s.top, d.lpa());
            if (d.getDriveStatus() != "Completed" && d.date().isValid() &&
                (!s.next.isValid() || d.date() < s.next)) s.next = d.date();
        }
        for (Summary &s : out) s.offers = offersByCompany.value(s.name);
        return out;
    }

    void refresh() {
        gridHost->setUpdatesEnabled(false);
        Theme::clearLayout(grid);
        colCount = Theme::columnsFor(width());
        int n = 0;
        for (const Summary &s : cachedSummaries) {
            if (sector != "All sectors" && Fmt::sectorGroup(s.name) != sector) continue;
            grid->addWidget(companyCard(s), n / colCount, n % colCount);
            ++n;
        }
        if (n == 0)
            grid->addWidget(Theme::emptyState("No partners in this sector yet",
                                              "Schedule a drive for a company in this sector to see it here."),
                            0, 0, 1, colCount);
        grid->setRowStretch(n / colCount + 1, 1);
        gridHost->setUpdatesEnabled(true);
    }

    QFrame *companyCard(const Summary &s) {
        auto *c = Theme::card();
        auto *v = Theme::vbox(c, 22, 14);
        auto *top = new QHBoxLayout;
        top->setSpacing(12);
        top->addWidget(Theme::logo(Util::initialsOf(s.name), s.top >= 20));
        auto *names = new QVBoxLayout;
        names->setSpacing(0);
        names->addWidget(Theme::label(s.name, "h2"));
        names->addWidget(Theme::label(Fmt::sectorOf(s.name), "muted", nullptr, true));
        top->addLayout(names, 1);
        top->addWidget(Theme::pill(s.top >= 20 ? "Tier 1 Partner" : "Core Partner", s.top >= 20 ? "success" : "amber"), 0, Qt::AlignTop);
        v->addLayout(top);
        v->addWidget(Theme::divider());

        auto *g = new QGridLayout;
        g->setHorizontalSpacing(20);
        g->setVerticalSpacing(10);
        auto fact = [&](int r, int col, const QString &k, const QString &val) {
            auto *b = new QVBoxLayout;
            b->setSpacing(2);
            b->addWidget(Theme::label(k, "eyebrow"));
            b->addWidget(Theme::label(val, "value"));
            g->addLayout(b, r, col);
        };
        fact(0, 0, "DRIVES", QString::number(s.drives));
        fact(0, 1, "OFFERS ISSUED", QString::number(s.offers));
        fact(1, 0, "TOP PACKAGE", Fmt::lpa(s.top));
        fact(1, 1, "NEXT DRIVE", s.next.isValid() ? Fmt::dateLong(s.next) : QString("—"));
        v->addLayout(g);

        auto *b = Theme::button("View Engagement Profile ➔", "soft");
        QObject::connect(b, &QPushButton::clicked, this, [this, s]() {
            QString text;
            for (const Company &d : ctx.store->drives())
                if (d.getName() == s.name)
                    text += QString("• %1 — %2 (%3, %4)\n").arg(d.getJobRole(), d.getPackageOffer(), d.getDriveStatus(), Fmt::dateLong(d.date()));
            QMessageBox::information(this, s.name, text.isEmpty() ? "No drives recorded." : text);
        });
        v->addWidget(b);
        return c;
    }

    void exportCsv() {
        QString out = Theme::csvLine({ "Company", "Sector", "Drives", "Offers", "Top package (LPA)" });
        for (const Summary &s : cachedSummaries)
            out += Theme::csvLine({ s.name, Fmt::sectorOf(s.name), QString::number(s.drives), QString::number(s.offers), QString::number(s.top, 'f', 1) });
        Theme::saveTextFile(this, "Export company directory", "companies.csv", "CSV files (*.csv)", out);
    }
};

#endif // DRIVES_PAGE_H
