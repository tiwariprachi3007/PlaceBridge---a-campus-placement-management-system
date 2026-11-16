#ifndef RECRUITER_PAGE_H
#define RECRUITER_PAGE_H

#include "theme.h"
#include "context.h"
#include "dialogs.h"
#include "SystemServices.h"
#include <QSet>

// Recruiter workspace for the signed-in Company: funnel numbers, candidate
// roster with persistent Shortlist, side-by-side comparison, interview schedule.
class RecruiterPage : public Theme::Page {
    Ctx ctx;
    QString company;
    QString filter = "All";
    QString selectedRole;
    QStringList companyRoles;              // this recruiter's own drive roles, read once
    QWidget *rosterHost = nullptr;
    QVBoxLayout *rosterLay = nullptr;

public:
    explicit RecruiterPage(const Ctx &c) : ctx(c) {
        company = ctx.user ? ctx.user->getName() : QString();
        build();
    }

private:
    QString firstRole() const {
        if (!selectedRole.isEmpty()) return selectedRole;
        return companyRoles.value(0);
    }

    void build() {
        const auto apps = ctx.store->applications();
        const auto events = ctx.store->events();
        const auto myDrives = ctx.store->drives();
        for (const Company &d : myDrives)
            if (d.getName() == company && !companyRoles.contains(d.getJobRole()))
                companyRoles.append(d.getJobRole());
        int total = 0, shortlisted = 0, interviews = 0, offers = 0;
        for (const Application &a : apps) {
            if (a.company != company) continue;
            ++total;
            if (a.stage() >= 1) ++shortlisted;
            if (a.stage() >= 3) ++offers;
        }
        for (const Event &e : events) if (e.company == company && e.type == "Interview") ++interviews;

        QString driveInfo = "No drive scheduled yet.";
        for (const Company &d : myDrives)
            if (d.getName() == company) { driveInfo = QString("%1 · %2 · drive on %3").arg(d.getJobRole(), d.getPackageOffer(), Fmt::dateLong(d.date())); break; }

        auto *post = Theme::button("+ Post New Job Role", "primary");
        auto *scheduleInterview = Theme::button("Schedule Interview", "soft");
        auto *exp = Theme::button("Export Shortlist", "ghost");
        auto *email = Theme::button("Email Shortlist", "ghost");
        QObject::connect(post, &QPushButton::clicked, this, [this]() {
            DriveDialog dialog(company, this);
            if (dialog.exec() != QDialog::Accepted) return;
            if (!ctx.store->addDrive(dialog.drive())) {
                QMessageBox::warning(this, "Job role not posted",
                    "The drive could not be saved. Check its date, package, and required fields.");
                return;
            }
            Theme::toast(this, "Job role posted; pending placement-office approval");
        });
        QObject::connect(scheduleInterview, &QPushButton::clicked, this, [this]() {
            EventDialog dialog(this, company);
            if (dialog.exec() != QDialog::Accepted) return;
            const Event event = dialog.toEvent();
            const QString conflict = ctx.store->interviewConflictReason(event);
            if (!conflict.isEmpty()) {
                QMessageBox::warning(this, "Interview conflict", conflict);
                return;
            }
            if (!ctx.store->addEvent(event)) {
                QMessageBox::warning(this, "Interview not scheduled",
                    "The event could not be saved. Confirm its date and your company permissions.");
                return;
            }
            Theme::toast(this, "Interview scheduled; applicants have been notified");
        });
        QObject::connect(exp, &QPushButton::clicked, this, [this]() { exportShortlist(); });
        QObject::connect(email, &QPushButton::clicked, this, [this]() { emailShortlist(); });
        lay->addWidget(Theme::hero("Corporate Recruitment Suite", company + " Campus Hiring",
                                   driveInfo, { post, scheduleInterview, exp, email }));

        auto *stats = new Theme::ResponsiveGrid(240);
        stats->addWidget(Theme::statCard("Total Applicants", QString::number(total), "Applications to your drives"));
        stats->addWidget(Theme::statCard("Shortlisted", QString::number(shortlisted), "Passed screening"));
        stats->addWidget(Theme::statCard("Interviews", QString::number(interviews), "Panels on the calendar"));
        stats->addWidget(Theme::statCard("Offers Extended", QString::number(offers), "Letters issued"));
        lay->addWidget(stats);

        // roster filter chips
        auto *bar = new Theme::ResponsiveGrid(140, 8);
        auto *group = new QButtonGroup(this);
        for (const QString &f : QStringList{ "All", "CGPA 8+", "Test 80+" }) {
            auto *chip = Theme::button(f, "chip");
            chip->setMinimumHeight(40);
            chip->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            chip->setChecked(f == "All");
            group->addButton(chip);
            bar->addWidget(chip);
            QObject::connect(chip, &QPushButton::clicked, this, [this, f]() { filter = f; refreshRoster(); });
        }
        auto *driveSelector = new QComboBox;
        driveSelector->addItem("Select role for shortlist", QString());
        for (const QString &roleName : companyRoles) driveSelector->addItem(roleName, roleName);
        if (!firstRole().isEmpty()) driveSelector->setCurrentIndex(
            qMax(1, driveSelector->findData(firstRole())));
        selectedRole = driveSelector->currentData().toString();
        connect(driveSelector, &QComboBox::currentIndexChanged, this, [this, driveSelector]() {
            selectedRole = driveSelector->currentData().toString();
            refreshRoster();
        });
        bar->addWidget(driveSelector);
        driveSelector->setMinimumWidth(0);
        driveSelector->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        lay->addWidget(bar);

        rosterHost = new QWidget;
        rosterLay = new QVBoxLayout(rosterHost);
        rosterLay->setContentsMargins(0, 0, 0, 0);
        rosterLay->setSpacing(18);
        lay->addWidget(rosterHost);
        refreshRoster();

        lay->addWidget(scheduleCard(events));
        lay->addStretch();
    }

    void refreshRoster() {
        Theme::clearLayout(rosterLay);
        const auto apps = ctx.store->applications();
        const QString activeRole = firstRole();
        QSet<QString> shortlistedNames;
        for (const Application &a : apps)
            if (a.company == company && a.role == activeRole && a.stage() >= 1)
                shortlistedNames.insert(a.student);
        QVector<Student> rows;
        for (const Student &s : ctx.store->students()) {
            if (filter == "CGPA 8+" && s.cgpaValue() < 8.0) continue;
            if (filter == "Test 80+" && s.testValue() < 80) continue;
            rows.append(s);
        }
        std::sort(rows.begin(), rows.end(), [](const Student &a, const Student &b) { return a.cgpaValue() > b.cgpaValue(); });

        auto *t = Theme::table({ "CANDIDATE", "BRANCH", "CGPA", "SKILLS", "ONLINE TEST", "ACTIONS" });
        t->setRowCount(rows.size());
        for (int i = 0; i < rows.size(); ++i) {
            const Student s = rows[i];
            auto *who = new QWidget;
            auto *h = new QHBoxLayout(who);
            h->setContentsMargins(8, 0, 8, 0);
            h->setSpacing(10);
            h->addWidget(Theme::logo(s.initials(), i % 2 == 1, 36));
            auto *nm = new QVBoxLayout;
            nm->setSpacing(0);
            nm->addWidget(Theme::label(s.getName(), "h3"));
            nm->addWidget(Theme::label(s.getRollNo(), "muted"));
            h->addLayout(nm);
            h->addStretch();
            t->setCellWidget(i, 0, who);
            Theme::setText(t, i, 1, s.branch());
            Theme::setText(t, i, 2, s.getCgpa(), true);
            Theme::setText(t, i, 3, s.skillList().mid(0, 3).join(", "));
            Theme::setText(t, i, 4, s.testValue() >= 0 ? QString("%1 / 100").arg(s.testValue()) : "—");

            QPushButton *act;
            const QString role = activeRole;
            if (shortlistedNames.contains(s.getName())) {
                act = Theme::button("Shortlisted ✓", "done");
                act->setEnabled(false);
            } else {
                act = Theme::button("Shortlist", "primary");
                act->setEnabled(!role.isEmpty());
                QObject::connect(act, &QPushButton::clicked, this, [this, s]() {
                    const QString role = firstRole();
                    if (role.isEmpty() || !ctx.store->shortlistCandidate(s.getEmail(), company, role)) {
                        QMessageBox::warning(this, "Candidate not shortlisted",
                            "Only a candidate who applied to one of your drives can be shortlisted.");
                        return;
                    }
                    Theme::toast(this, s.getName() + " shortlisted");
                });
            }
            auto *resume = Theme::button("Resume", "soft");
            QObject::connect(resume, &QPushButton::clicked, this, [this, s, apps]() {
                StudentInfoDialog dlg(s, apps, this);
                dlg.exec();
            });
            t->setCellWidget(i, 5, Theme::cellRow({ act, resume }));
        }
        Theme::fitTable(t);
        rosterLay->addWidget(Theme::tableCard("Candidate Roster", QString("%1 candidates match this filter, ranked by CGPA.").arg(rows.size()), nullptr, t));

        if (rows.size() >= 2) rosterLay->addWidget(comparisonCard(rows[0], rows[1]));
        rosterLay->addWidget(kanbanCard(apps));
    }

    QWidget *kanbanCard(const QVector<Application> &applications) {
        auto *card = Theme::card();
        auto *layout = Theme::vbox(card, 18, 12);
        layout->addWidget(Theme::sectionHead("Hiring Kanban", "Move candidates through your recruitment stages."));
        auto *columns = new Theme::ResponsiveGrid(210, 12);
        const QStringList stages = { "Applied", "Shortlisted", "Interview", "Offer" };
        for (const QString &stage : stages) {
            auto *column = new QFrame;
            column->setObjectName("softCard");
            auto *columnLayout = Theme::vbox(column, 12, 8);
            int count = 0;
            for (const Application &application : applications) {
                if (application.company != company || application.status.compare(stage, Qt::CaseInsensitive) != 0)
                    continue;
                ++count;
                auto *candidate = new QFrame;
                candidate->setObjectName("surfaceCard");
                auto *candidateLayout = Theme::vbox(candidate, 10, 5);
                candidateLayout->addWidget(Theme::label(application.student, "h3"));
                candidateLayout->addWidget(Theme::label(application.role, "muted", nullptr, true));
                auto *actions = new QHBoxLayout;
                auto *advance = new QComboBox;
                advance->addItems(stages + QStringList{ "Rejected" });
                advance->setCurrentText(stage);
                connect(advance, &QComboBox::currentTextChanged, this,
                    [this, application, stage](const QString &next) {
                        if (next == stage) return;
                        if (!ctx.store->updateApplicationStatus(application.student, application.company,
                                                                application.role, next))
                            QMessageBox::warning(this, "Stage not updated",
                                "The application stage could not be changed.");
                    });
                auto *score = Theme::button("Scorecard", "soft");
                score->setEnabled(stage == "Interview");
                connect(score, &QPushButton::clicked, this, [this, application]() {
                    bool ok = false;
                    const QString value = QInputDialog::getMultiLineText(
                        this, "Interview scorecard",
                        "Enter interviewer notes and scores:", application.scorecard, &ok);
                    if (!ok) return;
                    if (!ctx.store->saveInterviewScorecard(application.student, application.company,
                                                           application.role, value))
                        QMessageBox::warning(this, "Scorecard not saved",
                            "Scorecards must contain notes and require an active application.");
                });
                actions->addWidget(advance, 1);
                actions->addWidget(score);
                candidateLayout->addLayout(actions);
                columnLayout->addWidget(candidate);
            }
            columnLayout->insertWidget(0, Theme::label(QString("%1 · %2").arg(stage).arg(count), "h3"));
            columnLayout->addStretch();
            columns->addWidget(column);
        }
        layout->addWidget(columns);
        return card;
    }

    QWidget *comparisonCard(const Student &a, const Student &b) {
        auto *c = Theme::card();
        auto *v = Theme::vbox(c, 22, 14);
        v->addWidget(Theme::sectionHead("Candidate Comparison", "Top two candidates in the current view, side by side."));
        auto *row = new QHBoxLayout;
        row->setSpacing(16);
        for (const Student *s : { &a, &b }) {
            auto *box = new QFrame;
            box->setObjectName("softCard");
            auto *bv = Theme::vbox(box, 18, 6);
            auto *top = new QHBoxLayout;
            top->addWidget(Theme::logo(s->initials(), s == &a, 40));
            auto *nm = new QVBoxLayout;
            nm->setSpacing(0);
            nm->addWidget(Theme::label(s->getName(), "h3"));
            nm->addWidget(Theme::label(s->branch(), "muted"));
            top->addLayout(nm, 1);
            bv->addLayout(top);
            bv->addWidget(Theme::label(QString("CGPA %1   ·   Test %2").arg(s->getCgpa(), s->testValue() >= 0 ? QString::number(s->testValue()) : "—"), "value"));
            bv->addWidget(Theme::label(s->skillList().join(", "), "muted", nullptr, true));
            row->addWidget(box, 1);
        }
        v->addLayout(row);
        return c;
    }

    QWidget *scheduleCard(const QVector<Event> &events) {
        auto *c = Theme::card();
        auto *v = Theme::vbox(c, 22, 12);
        v->addWidget(Theme::sectionHead("Interview Schedule", "Panels booked for " + company + "."));
        int n = 0;
        for (const Event &e : events) {
            if (e.company != company || e.type != "Interview") continue;
            ++n;
            auto *row = new QHBoxLayout;
            row->setSpacing(14);
            auto *time = Theme::label(e.date.toString("dd MMM") + "  " + e.time, "h3");
            time->setMinimumWidth(110);
            row->addWidget(time);
            auto *t = new QVBoxLayout;
            t->setSpacing(0);
            QString who = e.title;
            who.remove("Interview: ");
            t->addWidget(Theme::label(who, "h3"));
            t->addWidget(Theme::label(e.detail, "muted"));
            row->addLayout(t, 1);
            row->addWidget(Theme::statusPill(e.state));
            v->addLayout(row);
        }
        if (n == 0) v->addWidget(Theme::label("No interviews scheduled yet.", "muted"));
        return c;
    }

    void exportShortlist() {
        QString out = Theme::csvLine({ "Name", "E-mail", "Branch", "CGPA", "Test score", "Status" });
        const auto apps = ctx.store->applications();
        for (const Student &s : ctx.store->students()) {
            QString applicationStatus;
            for (const Application &application : apps)
                if (application.student == s.getName() && application.company == company &&
                    application.stage() >= 1) {
                    applicationStatus = application.status;
                    break;
                }
            if (!applicationStatus.isEmpty())
                out += Theme::csvLine({ s.getName(), s.getEmail(), s.branch(), s.getCgpa(),
                                        s.getTestScore(), applicationStatus });
        }
        Theme::saveTextFile(this, "Export shortlist", "shortlist.csv", "CSV files (*.csv)", out);
    }

    void emailShortlist() {
        QDialog dialog(this);
        dialog.setWindowTitle("Email shortlist (mock)");
        dialog.setMinimumWidth(520);
        auto *layout = new QVBoxLayout(&dialog);
        auto *form = new QFormLayout;
        auto *to = new QLineEdit;
        auto *subject = new QLineEdit(company + " candidate shortlist");
        auto *body = new QTextEdit;
        body->setPlainText("Hello,\n\nPlease find the shortlisted candidates for " + company +
                           " below.\n\nRegards,\n" + company + " Campus Hiring");
        form->addRow("To", to);
        form->addRow("Subject", subject);
        form->addRow("Body", body);
        layout->addLayout(form);
        auto *save = Theme::button("Save as .eml", "primary");
        auto *close = Theme::button("Close", "soft");
        auto *buttons = new QHBoxLayout;
        buttons->addStretch();
        buttons->addWidget(close);
        buttons->addWidget(save);
        layout->addLayout(buttons);
        connect(close, &QPushButton::clicked, &dialog, &QDialog::reject);
        connect(save, &QPushButton::clicked, &dialog, [&]() {
            const QString emailError = Validator::emailError(to->text());
            if (!emailError.isEmpty()) {
                QMessageBox::warning(&dialog, "Invalid recipient", emailError);
                return;
            }
            const QString path = QFileDialog::getSaveFileName(this, "Save mock e-mail",
                QDir::homePath() + "/shortlist.eml", "E-mail message (*.eml)");
            if (path.isEmpty()) return;
            QString error;
            if (!MockMailTransport::saveDraft(path, to->text(), subject->text(),
                                              body->toPlainText(), &error)) {
                QMessageBox::critical(&dialog, "Save failed", error);
                return;
            }
            dialog.accept();
        });
        dialog.exec();
    }
};

#endif // RECRUITER_PAGE_H
