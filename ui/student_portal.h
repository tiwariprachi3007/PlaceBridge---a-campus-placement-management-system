#ifndef STUDENT_PORTAL_H
#define STUDENT_PORTAL_H

#include "theme.h"
#include "context.h"
#include "dialogs.h"
#include <QSettings>
#include <QPdfWriter>
#include <QTextDocument>
#include <QSaveFile>
#include <QDesktopServices>
#include <QUrl>
#include <QHash>
#include <QSet>
#include <QScrollArea>
#include <QScreen>
#include <QGuiApplication>

// Student's own workspace: profile, applications, recommended drives.
// `compact` stacks everything in one column (used by the Mobile View window).
class StudentPortalPage : public Theme::Page {
    Ctx ctx;
    bool compact;
    QString email;
    QBoxLayout *responsiveContentLayout = nullptr;

public:
    StudentPortalPage(const Ctx &c, bool compactMode = false) : ctx(c), compact(compactMode) {
        if (auto *me = dynamic_cast<Student *>(ctx.user)) email = me->getEmail();
        if (compact) lay->setContentsMargins(14, 14, 14, 20);
        build();
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        Theme::Page::resizeEvent(event);
        if (responsiveContentLayout)
            responsiveContentLayout->setDirection(width() < 760
                ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    }

private:
    void build() {
        const auto found = ctx.store->findStudent(email);
        if (!found) {
            lay->addWidget(Theme::label("Your student profile could not be found. Please contact the placement office.", "muted", nullptr, true));
            return;
        }
        const Student s = *found;
        const auto allApps = ctx.store->applications();

        QList<QPushButton *> actions;
        if (!compact) {
            auto *resume = Theme::button("Upload Updated Resume", "ghost");
            auto *mobile = Theme::button("Mobile View", "primary");
            auto *edit = Theme::button("Edit Profile", "ghost");
            auto *documents = Theme::button("Document Vault", "ghost");
            QObject::connect(resume, &QPushButton::clicked, this, [this]() { uploadResume(); });
            QObject::connect(mobile, &QPushButton::clicked, this, [this]() { openMobile(); });
            QObject::connect(edit, &QPushButton::clicked, this, [this, s]() { editProfile(s); });
            QObject::connect(documents, &QPushButton::clicked, this, [this]() { documentVault(); });
            actions = { mobile, resume, edit, documents };
        }
        lay->addWidget(Theme::hero("Candidate Career Portal", compact ? "My Profile" : "Student Profile & Applications",
                                   "Track your verified placement status, active selection rounds and recommended drives.", actions));

        QWidget *profile = profileCard(s);
        QWidget *right = new QWidget;
        auto *rv = new QVBoxLayout(right);
        rv->setContentsMargins(0, 0, 0, 0);
        rv->setSpacing(18);
        rv->addWidget(applicationsCard(s, allApps));
        rv->addWidget(recommendedCard(s, allApps));

        if (compact) {
            lay->addWidget(profile);
            lay->addWidget(right);
        } else {
            responsiveContentLayout = new QBoxLayout(QBoxLayout::LeftToRight);
            responsiveContentLayout->setSpacing(18);
            responsiveContentLayout->addWidget(profile, 2, Qt::AlignTop);
            responsiveContentLayout->addWidget(right, 3, Qt::AlignTop);
            lay->addLayout(responsiveContentLayout);
            responsiveContentLayout->setDirection(width() < 760
                ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
        }
        lay->addStretch();
    }

    QString resumePath() const { return QSettings().value("resume/" + email).toString(); }

    QWidget *profileCard(const Student &s) {
        auto *c = Theme::card();
        auto *v = Theme::vbox(c, 24, 14);
        v->addWidget(Theme::logo(s.initials(), true, 84), 0, Qt::AlignHCenter);
        auto *nm = Theme::label(s.getName(), "bigName");
        nm->setAlignment(Qt::AlignCenter);
        v->addWidget(nm);
        auto *meta = Theme::label(s.getCourse() + (s.getRollNo().isEmpty() ? "" : "  ·  Roll No " + s.getRollNo()), "muted", nullptr, true);
        meta->setAlignment(Qt::AlignCenter);
        v->addWidget(meta);
        v->addWidget(Theme::pill(s.isVerified() ? "Profile Verified" : "Verification Pending",
                                 s.isVerified() ? "success" : "warning"), 0, Qt::AlignHCenter);
        v->addWidget(Theme::divider());

        auto fact = [&](const QString &k, QWidget *val) {
            auto *row = new QHBoxLayout;
            auto *key = Theme::label(k, "eyebrow", nullptr, true);
            key->setMinimumWidth(0);
            row->addWidget(key, 1);
            if (auto *value = qobject_cast<QLabel *>(val)) {
                value->setWordWrap(true);
                value->setMinimumWidth(0);
            }
            row->addWidget(val, 1, Qt::AlignRight);
            v->addLayout(row);
        };
        fact("CUMULATIVE CGPA", Theme::label(s.cgpaValue() >= 0 ? s.getCgpa() + " / 10.0" : "Not recorded", "value"));
        fact("ONLINE TEST SCORE", Theme::label(s.testValue() >= 0 ? QString::number(s.testValue()) + " / 100" : "Not taken", "value"));
        fact("PLACEMENT STATUS", Theme::statusPill(s.getStatus()));
        fact("CAMPUS E-MAIL", Theme::label(s.getEmail(), "muted"));

        const QString resume = resumePath();          // one QSettings read, not three
        const QStringList skills = s.skillList();
        int filled = 0;
        filled += !s.getName().isEmpty();
        filled += !s.getCourse().isEmpty();
        filled += s.cgpaValue() >= 0;
        filled += !s.getRollNo().isEmpty();
        filled += !skills.isEmpty();
        filled += s.testValue() >= 0;
        filled += !resume.isEmpty();
        filled += !s.getEmail().isEmpty();
        const int pct = qRound(100.0 * filled / 8);
        v->addWidget(Theme::metricRow("Profile completeness",
                                      resume.isEmpty() ? "Upload a resume to reach 100%"
                                                       : "Resume: " + QFileInfo(resume).fileName(),
                                      QString("%1%").arg(pct), pct, "forest"));

        if (!skills.isEmpty()) {
            v->addWidget(Theme::label("VERIFIED SKILLS", "eyebrow"));
            auto *sk = new QHBoxLayout;
            sk->setSpacing(6);
            for (const QString &x : skills.mid(0, compact ? 2 : 4)) sk->addWidget(Theme::label(x, "skill"));
            sk->addStretch();
            v->addLayout(sk);
        }
        return c;
    }

    QWidget *applicationsCard(const Student &s, const QVector<Application> &all) {
        auto *c = Theme::card();
        auto *v = Theme::vbox(c, 22, 14);
        v->addWidget(Theme::sectionHead("Active Applications", "Your progress across every corporate drive."));
        QHash<QString, QString> packageByDrive;
        for (const Company &d : ctx.store->drives())
            packageByDrive.insert(Application::driveKey(d.getName(), d.getJobRole()), d.getPackageOffer());
        const auto allEvents = ctx.store->events();
        int n = 0;
        for (const Application &a : all) {
            if (a.student != s.getName()) continue;
            ++n;
            const QString pkg = packageByDrive.value(a.driveKey());
            const QString note = a.stage() >= 3 ? "Offer issued. Review and accept it with the placement office."
                               : a.stage() == 2 ? "Interview rounds in progress."
                               : a.stage() == 1 ? "Shortlisted. Await your interview schedule."
                                                : "Application is being processed.";
            auto *row = new QHBoxLayout;
            row->setSpacing(12);
            row->addWidget(Theme::logo(Util::initialsOf(a.company), n % 2 == 0), 0, Qt::AlignTop);
            auto *t = new QVBoxLayout;
            t->setSpacing(1);
            t->addWidget(Theme::label(a.company, "h3"));
            t->addWidget(Theme::label(a.role + (pkg.isEmpty() ? "" : "  ·  " + pkg), "muted", nullptr, true));
            t->addWidget(Theme::label(note, "muted", nullptr, true));
            row->addLayout(t, 1);
            row->addWidget(Theme::statusPill(a.status), 0, Qt::AlignTop);
            if (a.offerDecision == "Accepted") {
                row->addWidget(Theme::pill("Offer Accepted", "success"), 0, Qt::AlignTop);
            } else if (a.offerDecision == "Declined") {
                row->addWidget(Theme::pill("Offer Declined", "neutral"), 0, Qt::AlignTop);
            } else if (a.stage() == 3) {
                auto *accept = Theme::button("Accept offer", "primary");
                auto *decline = Theme::button("Decline", "danger");
                QObject::connect(accept, &QPushButton::clicked, this, [this, s, a]() {
                    if (QMessageBox::question(this, "Accept offer",
                        QString("Accept the offer from %1? Other open applications will close.").arg(a.company))
                        == QMessageBox::Yes && ctx.store->acceptOffer(email, a.company, a.role)) {
                        generateOfferLetter(a);
                        Theme::toast(this, "Offer accepted");
                    }
                });
                QObject::connect(decline, &QPushButton::clicked, this, [this, a]() {
                    if (ctx.store->declineOffer(email, a.company, a.role))
                        Theme::toast(this, "Offer declined");
                });
                row->addWidget(accept, 0, Qt::AlignTop);
                row->addWidget(decline, 0, Qt::AlignTop);
            }
            if (a.stage() >= 0 && a.stage() < 3) {
                auto *withdraw = Theme::button("Withdraw", "danger");
                QObject::connect(withdraw, &QPushButton::clicked, this, [this, s, a]() {
                    if (QMessageBox::question(this, "Withdraw application",
                            QString("Withdraw your application to %1?").arg(a.company)) == QMessageBox::Yes &&
                        ctx.store->withdrawApplication(s.getName(), a.company, a.role))
                        Theme::toast(this, "Application withdrawn");
                });
                row->addWidget(withdraw, 0, Qt::AlignTop);
            }
            if (a.stage() == 2) {
                for (const Event &event : allEvents) {
                    if (event.company != a.company || event.type.compare("Interview", Qt::CaseInsensitive) != 0 ||
                        event.date < QDate::currentDate()) continue;
                    auto *book = Theme::button("Book " + event.date.toString("dd MMM") + " slot", "soft");
                    QObject::connect(book, &QPushButton::clicked, this, [this, a, event]() {
                        if (!ctx.store->bookInterviewSlot(email, a.company, a.role, event.title))
                            QMessageBox::information(this, "Slot unavailable",
                                "This slot could not be booked; it may already be occupied.");
                        else Theme::toast(this, "Interview slot booked");
                    });
                    row->addWidget(book, 0, Qt::AlignTop);
                    break;
                }
            }
            if (a.stage() >= 2 && a.stage() != 3 && a.stage() != -1) {
                auto *feedback = Theme::button("Drive feedback", "soft");
                QObject::connect(feedback, &QPushButton::clicked, this, [this, a]() {
                    bool ok = false;
                    const int rating = QInputDialog::getInt(this, "Drive feedback", "Rating (1–5):",
                                                             5, 1, 5, 1, &ok);
                    if (!ok) return;
                    const QString comments = QInputDialog::getMultiLineText(
                        this, "Drive feedback", "Comments:");
                    if (!ctx.store->submitDriveFeedback(email, a.company, a.role, rating, comments))
                        QMessageBox::warning(this, "Feedback not saved", "Feedback could not be saved.");
                    else Theme::toast(this, "Thank you for your feedback");
                });
                row->addWidget(feedback, 0, Qt::AlignTop);
            }
            v->addLayout(row);
            v->addWidget(Theme::divider());
        }
        if (n == 0) v->addWidget(Theme::label("No applications yet. Apply to an open drive below.", "muted", nullptr, true));
        return c;
    }

    QWidget *recommendedCard(const Student &s, const QVector<Application> &all) {
        auto *c = Theme::card();
        auto *v = Theme::vbox(c, 22, 14);
        auto *more = Theme::button("All Drives ➔", "link");
        QObject::connect(more, &QPushButton::clicked, this, [this]() { if (ctx.navigate) ctx.navigate("Drives"); });
        v->addWidget(Theme::sectionHead("Recommended Drives", "Matches your CGPA, branch and listed skills.", more));
        const QHash<QString, QString> eligibility = ctx.store->eligibilityForVisibleDrives(email);
        QSet<QString> appliedKeys;
        for (const Application &a : all)
            if (a.student == s.getName()) appliedKeys.insert(a.driveKey());
        static const QRegularExpression separators("[^a-z0-9]+");
        int n = 0;
        for (const Company &d : ctx.store->drives()) {
            if (d.getDriveStatus() != "Open" || !d.isApproved()) continue;
            const QString key = Application::driveKey(d.getName(), d.getJobRole());
            if (appliedKeys.contains(key) || !eligibility.value(key).isEmpty()) continue;
            const QStringList tokens = (d.getJobRole() + " " + d.getName()).toCaseFolded().split(
                separators, Qt::SkipEmptyParts);
            bool skillMatch = false;
            for (const QString &skill : s.skillList())
                for (const QString &token : tokens)
                    if (skill.compare(token, Qt::CaseInsensitive) == 0) skillMatch = true;
            if (!s.skillList().isEmpty() && !skillMatch) continue;
            if (n++ == 3) break;
            auto *row = new QHBoxLayout;
            row->setSpacing(12);
            row->addWidget(Theme::logo(d.initials(), d.lpa() >= 20));
            auto *t = new QVBoxLayout;
            t->setSpacing(0);
            t->addWidget(Theme::label(d.getName(), "h3"));
            t->addWidget(Theme::label(d.getJobRole() + "  ·  " + Fmt::lpa(d.lpa()), "muted", nullptr, true));
            row->addLayout(t, 1);
            auto *apply = Theme::button("Quick Apply", "soft");
            const Student me = s;
            QObject::connect(apply, &QPushButton::clicked, this, [this, me, d]() {
                const QString reason = ctx.store->eligibilityForDrive(email, d.getName(), d.getJobRole());
                if (!reason.isEmpty()) {
                    QMessageBox::information(this, "Not eligible", reason);
                    return;
                }
                if (ctx.store->addApplication({ me.getName(), d.getName(), d.getJobRole(), "Applied" }))
                    Theme::toast(this, QString("Application submitted to %1").arg(d.getName()));
                else QMessageBox::warning(this, "Application not submitted",
                                           "Your application could not be saved or you are not eligible.");
            });
            row->addWidget(apply);
            v->addLayout(row);
        }
        if (n == 0) v->addWidget(Theme::label("No new drives match your profile right now.", "muted", nullptr, true));
        return c;
    }

    void uploadResume() {
        const QString path = QFileDialog::getOpenFileName(this, "Select your resume", QDir::homePath(),
                                                          "Documents (*.pdf *.doc *.docx)");
        if (path.isEmpty()) return;
        QFile source(path);
        if (!source.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(this, "Upload failed", source.errorString());
            return;
        }
        const QByteArray contents = source.readAll();
        if (!ctx.store->addStudentDocument(email, QFileInfo(path).fileName(), contents)) {
            QMessageBox::warning(this, "Upload failed", "Document could not be saved (maximum 25 MB).");
            return;
        }
        QSettings().setValue("resume/" + email, path);
        emit ctx.store->changed();   // refresh the completeness bar
        Theme::toast(this, "Resume uploaded successfully");
    }

    void editProfile(Student student) {
        QDialog dialog(this);
        dialog.setWindowTitle("Edit student profile");
        auto *form = new QFormLayout(&dialog);
        auto *skills = new QLineEdit(student.getSkills());
        auto *projects = new QTextEdit(student.getProjects());
        auto *achievements = new QTextEdit(student.getAchievements());
        auto *links = new QLineEdit(student.getLinks());
        auto *backlogs = new QSpinBox;
        backlogs->setRange(0, 99);
        backlogs->setValue(student.getBacklogs());
        auto *batch = new QSpinBox;
        batch->setRange(0, QDate::currentDate().year() + 6);
        batch->setSpecialValueText("Not specified");
        batch->setValue(student.getBatchYear());
        form->addRow("Skills (comma separated)", skills);
        form->addRow("Projects", projects);
        form->addRow("Achievements", achievements);
        form->addRow("Links", links);
        form->addRow("Active backlogs", backlogs);
        form->addRow("Graduation year", batch);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
        form->addRow(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted) return;
        student.setSkills(skills->text());
        student.setProjects(projects->toPlainText());
        student.setAchievements(achievements->toPlainText());
        student.setLinks(links->text());
        student.setBacklogs(backlogs->value());
        student.setBatchYear(batch->value());
        if (!ctx.store->updateStudentProfile(student))
            QMessageBox::warning(this, "Profile not saved", "The profile could not be saved.");
        else Theme::toast(this, "Profile saved");
    }

    void documentVault() {
        QDialog dialog(this);
        dialog.setWindowTitle("Document vault");
        dialog.resize(520, 400);
        auto *layout = new QVBoxLayout(&dialog);
        auto *list = new QListWidget;
        const auto documents = ctx.store->studentDocuments(email);
        for (const VaultDocument &document : documents) {
            auto *item = new QListWidgetItem(
                document.name + (document.verified ? "  ·  Verified" : "  ·  Pending verification"));
            item->setData(Qt::UserRole, document.id);
            list->addItem(item);
        }
        auto *empty = Theme::emptyState("No documents uploaded yet",
                                        "Upload a resume or another document to keep it here.");
        empty->setVisible(documents.isEmpty());
        list->setVisible(!documents.isEmpty());
        auto *upload = Theme::button("Upload document", "primary");
        auto *preview = Theme::button("Preview selected", "soft");
        auto *close = Theme::button("Close", "ghost");
        auto *buttons = new QHBoxLayout;
        buttons->addWidget(upload);
        buttons->addWidget(preview);
        buttons->addStretch();
        buttons->addWidget(close);
        layout->addWidget(empty, 1);
        layout->addWidget(list, 1);
        layout->addLayout(buttons);
        connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
        bool uploaded = false;
        connect(upload, &QPushButton::clicked, &dialog, [this, &dialog, &uploaded]() {
            const QString path = QFileDialog::getOpenFileName(this, "Add document", QDir::homePath());
            if (path.isEmpty()) return;
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) {
                QMessageBox::critical(this, "Upload failed", file.errorString());
                return;
            }
            if (!ctx.store->addStudentDocument(email, QFileInfo(path).fileName(), file.readAll()))
                QMessageBox::warning(this, "Upload failed", "The document is empty or larger than 25 MB.");
            else {
                uploaded = true;
                dialog.accept();
            }
        });
        connect(preview, &QPushButton::clicked, &dialog, [this, list, documents]() {
            const auto *selected = list->currentItem();
            if (!selected) {
                QMessageBox::information(this, "Select a document", "Choose a document from the vault first.");
                return;
            }
            const qint64 id = selected->data(Qt::UserRole).toLongLong();
            for (const VaultDocument &document : documents) {
                if (document.id != id) continue;
                const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                    + "/PlaceBridgeVault";
                if (!QDir().mkpath(dir)) {
                    QMessageBox::critical(this, "Preview failed", "Could not create a temporary folder.");
                    return;
                }
                const QString path = dir + "/" + QFileInfo(document.name).fileName();
                QSaveFile file(path);
                if (!file.open(QIODevice::WriteOnly) || file.write(document.content) != document.content.size() ||
                    !file.commit()) {
                    QMessageBox::critical(this, "Preview failed", "Could not stage this document.");
                    return;
                }
                if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
                    QMessageBox::warning(this, "Preview failed",
                                         "No application can open this document. Check that an app is "
                                         "associated with this file type.");
                return;
            }
        });
        dialog.exec();
        if (uploaded) Theme::toast(this, "Document added to your vault");
    }

    void generateOfferLetter(const Application &application) {
        const QString path = QFileDialog::getSaveFileName(this, "Save offer letter",
            QDir::homePath() + "/offer-letter.pdf", "PDF files (*.pdf)");
        if (path.isEmpty()) return;
        QPdfWriter pdf(path);
        pdf.setTitle("PlaceBridge offer letter");
        QTextDocument doc;
        doc.setHtml(QString("<h1>Placement Offer Acceptance</h1><p>Student: %1</p>"
                            "<p>Company: %2</p><p>Role: %3</p><p>Status: Accepted</p>")
                        .arg(application.student.toHtmlEscaped(), application.company.toHtmlEscaped(),
                             application.role.toHtmlEscaped()));
        doc.print(&pdf);
    }


    // Shows the portal at phone width so a student can check how their profile
    // reads on a small screen. No hard-coded background here: the dialog inherits
    // the active theme, which the old inline style sheet overrode in dark mode.
    void openMobile() {
        QDialog dialog(this);
        dialog.setWindowTitle("Mobile preview");

        const QRect available = QGuiApplication::primaryScreen()->availableGeometry();
        dialog.resize(qMin(420, available.width() - 32), qMin(780, available.height() - 64));
        dialog.setMinimumSize(280, 350);

        auto *layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(0, 0, 0, 0);

        auto *compactPortal = new StudentPortalPage(ctx, true);
        compactPortal->setObjectName("mobilePortalPage");
        layout->addWidget(compactPortal);   // Theme::Page already scrolls
        dialog.exec();
    }
};

#endif // STUDENT_PORTAL_H
