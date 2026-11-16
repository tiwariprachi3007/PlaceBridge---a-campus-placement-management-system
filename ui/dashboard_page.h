#ifndef DASHBOARD_PAGE_H
#define DASHBOARD_PAGE_H

#include "theme.h"
#include "context.h"
#include "dialogs.h"
#include <QMap>
#include <QSet>

// Admin home: live numbers computed from the data files.
class DashboardPage : public Theme::Page {
    Ctx ctx;

public:
    explicit DashboardPage(const Ctx &c) : ctx(c) { build(); }

private:
    void build() {
        DataStore *st = ctx.store;
        const auto students = st->students();
        const auto drives = st->drives();
        const auto apps = st->applications();
        const auto events = st->events();

        int placed = 0, pending = 0, active = 0;
        for (const Student &s : students) if (s.isPlaced()) ++placed;
        for (const Application &a : apps) {
            const QString s = a.status.toLower();
            if (s.contains("review") || s == "applied") ++pending;
        }
        QSet<QString> companies, branches;
        double sum = 0, peak = 0;
        QString peakCo;
        for (const Company &d : drives) {
            companies.insert(d.getName());
            if (d.getDriveStatus() != "Completed") ++active;
            sum += d.lpa();
            if (d.lpa() > peak) { peak = d.lpa(); peakCo = d.getName(); }
        }
        for (const Student &s : students) branches.insert(s.branch());
        const double avg = drives.isEmpty() ? 0.0 : sum / drives.size();
        const int placedPct = students.isEmpty() ? 0 : qRound(100.0 * placed / students.size());

        // ---- hero
        auto *addBtn = Theme::button("+ Add Company", "primary");
        auto *schedBtn = Theme::button("Schedule Drive", "ghost");
        auto addDrive = [this]() {
            DriveDialog dlg(QString(), this);
            if (dlg.exec() == QDialog::Accepted) { ctx.store->addDrive(dlg.drive()); Theme::toast(this, "Drive scheduled"); }
        };
        QObject::connect(addBtn, &QPushButton::clicked, this, addDrive);
        QObject::connect(schedBtn, &QPushButton::clicked, this, addDrive);
        lay->addWidget(Theme::hero("Academic Season 2026 · Active Operations", "Operations Dashboard",
                                   QString("%1 company drives currently scheduled and %2 student applications pending verification.")
                                       .arg(active).arg(pending),
                                   { addBtn, schedBtn }));

        // ---- stat cards
        auto *stats = new Theme::ResponsiveGrid(240);
        stats->addWidget(Theme::statCard("Registered Candidates", QString::number(students.size()),
                                         QString("Across %1 engineering branches").arg(branches.size())));
        stats->addWidget(Theme::ringCard("Placement Rate", placedPct,
                                         QString("%1 of %2 candidates have accepted an offer")
                                             .arg(placed).arg(students.size()),
                                         "placed", placedPct >= 60 ? "success"
                                                 : placedPct >= 30 ? "accent" : "danger"));
        stats->addWidget(Theme::statCard("Partner Companies", QString::number(companies.size()),
                                         QString("%1 active campus drives").arg(active)));
        stats->addWidget(Theme::statCard("Average CTC Package", QString::number(avg, 'f', 1) + " LPA",
                                         peak > 0 ? QString("Peak package: %1 (%2)").arg(Fmt::lpa(peak), peakCo) : QString("No drives yet")));
        lay->addWidget(stats);

        // ---- pipeline + upcoming events
        auto *mid = new Theme::ResponsiveGrid(420);
        mid->addWidget(pipelineCard(apps));
        mid->addWidget(upcomingCard(events));
        lay->addWidget(mid);

        lay->addWidget(branchCard(students));
        lay->addWidget(recentTable(students, apps));
        lay->addStretch();
    }

    QWidget *pipelineCard(const QVector<Application> &apps) {
        auto *c = Theme::card();
        auto *v = Theme::vbox(c, 22, 16);
        v->addWidget(Theme::sectionHead("Recruitment Pipeline Flow",
                                        "Live movement of candidate applications across active selection stages.",
                                        Theme::pill("Active Season", "success")));
        int total = apps.size(), sh = 0, iv = 0, of = 0;
        for (const Application &a : apps) {
            if (a.stage() >= 1) ++sh;
            if (a.stage() >= 2) ++iv;
            if (a.stage() >= 3) ++of;
        }
        auto pct = [total](int n) { return total ? qRound(100.0 * n / total) : 0; };
        v->addWidget(Theme::metricRow("Applied", "Initial applications submitted", QString::number(total), 100));
        v->addWidget(Theme::metricRow("Shortlisted", "Test benchmarks passed", QString::number(sh), pct(sh)));
        v->addWidget(Theme::metricRow("In Interviews", "Technical & HR rounds", QString::number(iv), pct(iv), "forest"));
        v->addWidget(Theme::metricRow("Offers Extended", "Letters of intent issued", QString::number(of), pct(of), "forest"));
        v->addStretch();
        return c;
    }

    QWidget *upcomingCard(const QVector<Event> &events) {
        auto *c = Theme::card();
        auto *v = Theme::vbox(c, 22, 14);
        auto *all = Theme::button("All Drives ➔", "link");
        QObject::connect(all, &QPushButton::clicked, this, [this]() { if (ctx.navigate) ctx.navigate("Drives"); });
        v->addWidget(Theme::sectionHead("Upcoming Drive Events", QString(), all));

        int shown = 0;
        for (const Event &e : events) {
            // Completion lives in `state`; `type` only names the round
            // ("Assessment" / "Interview"), so the old type check never matched.
            if (e.date < QDate::currentDate()) continue;
            const QString state = e.state.trimmed();
            if (state.compare("Completed", Qt::CaseInsensitive) == 0 ||
                state.compare("Cancelled", Qt::CaseInsensitive) == 0) continue;
            if (shown++ == 3) break;
            auto *row = new QHBoxLayout;
            row->setSpacing(12);
            row->addWidget(Theme::logo(Util::initialsOf(e.company.isEmpty() ? e.title : e.company), shown % 2 == 0));
            auto *t = new QVBoxLayout;
            t->setSpacing(0);
            t->addWidget(Theme::label(e.title, "h3"));
            t->addWidget(Theme::label(e.date.toString("dd MMM") + " · " + e.detail, "muted"));
            row->addLayout(t, 1);
            row->addWidget(Theme::statusPill(e.state), 0, Qt::AlignVCenter);
            v->addLayout(row);
        }
        if (shown == 0) v->addWidget(Theme::label("No upcoming events. Use the Calendar page to schedule one.", "muted", nullptr, true));
        v->addStretch();
        return c;
    }

    QWidget *branchCard(const QVector<Student> &students) {
        auto *c = Theme::card();
        auto *v = Theme::vbox(c, 22, 16);
        v->addWidget(Theme::sectionHead("Placement Rate by Academic Branch",
                                        "Share of registered students per branch who have accepted an offer."));
        QMap<QString, QPair<int, int>> byBranch;   // branch -> (total, placed)
        for (const Student &s : students) {
            byBranch[s.branch()].first++;
            if (s.isPlaced()) byBranch[s.branch()].second++;
        }
        QVector<QPair<int, QString>> ranked;
        for (auto it = byBranch.begin(); it != byBranch.end(); ++it)
            ranked.append({ qRound(100.0 * it.value().second / it.value().first), it.key() });
        std::sort(ranked.begin(), ranked.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
        for (const auto &r : ranked)
            v->addWidget(Theme::metricRow(r.second, QString("%1 students").arg(byBranch[r.second].first),
                                          QString("%1% Placed").arg(r.first), r.first,
                                          r.first >= 60 ? "forest" : (r.first >= 30 ? "amber" : "danger")));
        if (ranked.isEmpty()) v->addWidget(Theme::label("No students registered yet.", "muted"));
        return c;
    }

    QWidget *recentTable(const QVector<Student> &students, const QVector<Application> &apps) {
        const int n = qMin(5, students.size());
        auto *t = Theme::table({ "CANDIDATE NAME", "BRANCH", "CGPA", "TARGET COMPANY", "SELECTION STATUS", "ACTIONS" });
        t->setRowCount(n);
        for (int i = 0; i < n; ++i) {
            const Student &s = students[students.size() - 1 - i];
            QString target = "—";
            for (const Application &a : apps) if (a.student == s.getName()) { target = a.company; break; }
            Theme::setText(t, i, 0, s.getName(), true);
            Theme::setText(t, i, 1, s.branch());
            Theme::setText(t, i, 2, s.getCgpa());
            Theme::setText(t, i, 3, target);
            t->setCellWidget(i, 4, Theme::cell(Theme::statusPill(s.getStatus())));
            auto *b = Theme::button("View Profile", "soft");
            QObject::connect(b, &QPushButton::clicked, this, [this, s, apps]() {
                StudentInfoDialog dlg(s, apps, this);
                dlg.exec();
            });
            t->setCellWidget(i, 5, Theme::cell(b));
        }
        Theme::fitTable(t);
        auto *all = Theme::button("Full Registry ➔", "link");
        QObject::connect(all, &QPushButton::clicked, this, [this]() { if (ctx.navigate) ctx.navigate("Students"); });
        return Theme::tableCard("Recent Candidate Submissions", "Most recently registered records from the data store.", all, t);
    }
};

#endif // DASHBOARD_PAGE_H
