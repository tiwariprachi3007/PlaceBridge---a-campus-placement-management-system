#ifndef DIALOGS_H
#define DIALOGS_H

#include "theme.h"
#include "context.h"
#include "SecurityManager.h"

// Shared look for every form dialog: serif title, form rows, Cancel / Save row.
class FormDialog : public QDialog {
protected:
    QFormLayout *form;
    QLabel *errorLabel;
    QPushButton *saveBtn;

    FormDialog(const QString &title, const QString &subtitle, const QString &saveText, QWidget *parent)
        : QDialog(parent) {
        setWindowTitle(title);
        setModal(true);
        setMinimumWidth(460);
        auto *v = new QVBoxLayout(this);
        v->setContentsMargins(28, 24, 28, 22);
        v->setSpacing(14);
        v->addWidget(Theme::label(title, "h2"));
        if (!subtitle.isEmpty()) v->addWidget(Theme::label(subtitle, "muted", nullptr, true));
        form = new QFormLayout;
        form->setSpacing(12);
        form->setLabelAlignment(Qt::AlignLeft);
        v->addLayout(form);
        errorLabel = Theme::label("", "error", nullptr, true);
        errorLabel->hide();
        v->addWidget(errorLabel);
        auto *row = new QHBoxLayout;
        row->addStretch();
        auto *cancel = Theme::button("Cancel", "soft");
        saveBtn = Theme::button(saveText, "primary");
        row->addWidget(cancel);
        row->addWidget(saveBtn);
        v->addLayout(row);
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    }

    void fail(const QString &msg) { errorLabel->setText(msg); errorLabel->show(); }
};

// ----------------------------------------------------------------------------
class StudentDialog : public FormDialog {
    QLineEdit *nameEdit, *emailEdit, *cgpaEdit, *rollEdit, *skillsEdit;
    QComboBox *courseBox;

public:
    StudentDialog(DataStore *store, QWidget *parent = nullptr)
        : FormDialog("Register New Candidate", "Adds the student to the placement registry.", "Save Candidate", parent) {
        nameEdit = new QLineEdit; nameEdit->setPlaceholderText("e.g. Rahul Sharma");
        emailEdit = new QLineEdit; emailEdit->setPlaceholderText("name@placebridge.edu");
        courseBox = new QComboBox; courseBox->setEditable(true);
        courseBox->addItems({ "B.Tech Computer Science", "B.Tech Information Technology",
                              "B.Tech Electronics & Comm", "B.Tech Electrical Eng", "B.Tech Mechanical Eng" });
        cgpaEdit = new QLineEdit; cgpaEdit->setPlaceholderText("e.g. 8.5 (out of 10)");
        rollEdit = new QLineEdit; rollEdit->setPlaceholderText("e.g. 21CS014");
        skillsEdit = new QLineEdit; skillsEdit->setPlaceholderText("Comma separated, e.g. Java, SQL, Cloud");
        form->addRow("Full name", nameEdit);
        form->addRow("Campus e-mail", emailEdit);
        form->addRow("Course", courseBox);
        form->addRow("CGPA", cgpaEdit);
        form->addRow("Roll number", rollEdit);
        form->addRow("Skills", skillsEdit);

        connect(saveBtn, &QPushButton::clicked, this, [this, store]() {
            const QString email = emailEdit->text().trimmed();
            const QString nameError = Validator::requiredTextError(nameEdit->text(), "the student's name");
            if (!nameError.isEmpty()) return fail(nameError);
            const QString emailError = Validator::studentEmailError(store, email);
            if (!emailError.isEmpty()) return fail(emailError);
            const QString g = cgpaEdit->text().trimmed();
            const QString cgpaError = Validator::cgpaError(g);
            if (!cgpaError.isEmpty()) return fail(cgpaError);
            const QString rollError = Validator::rollNumberError(store, rollEdit->text());
            if (!rollError.isEmpty()) return fail(rollError);
            accept();
        });
    }

    Student student() const {
        const QString g = cgpaEdit->text().trimmed();
        return Student(nameEdit->text().trimmed(), emailEdit->text().trimmed(),
                       courseBox->currentText().trimmed(), g.isEmpty() ? "N/A" : g, "Registered",
                       rollEdit->text().trimmed(), skillsEdit->text().trimmed(), "");
    }
};

// ----------------------------------------------------------------------------
class DriveDialog : public FormDialog {
    QLineEdit *companyEdit, *roleEdit, *packageEdit, *branchesEdit;
    QTextEdit *descriptionEdit, *eligibilityEdit, *roundsEdit;
    QDateEdit *dateEdit;
    QDoubleSpinBox *cgpaSpin;
    QComboBox *statusBox;

public:
    DriveDialog(const QString &presetCompany = QString(), QWidget *parent = nullptr)
        : FormDialog("Schedule New Drive", "Publishes a recruitment drive to students and the calendar.", "Schedule Drive", parent) {
        companyEdit = new QLineEdit(presetCompany); companyEdit->setPlaceholderText("e.g. Google, Microsoft, TCS");
        if (!presetCompany.isEmpty()) companyEdit->setReadOnly(true);
        roleEdit = new QLineEdit; roleEdit->setPlaceholderText("e.g. Software Engineer");
        packageEdit = new QLineEdit; packageEdit->setPlaceholderText("e.g. 12 LPA");
        branchesEdit = new QLineEdit;
        branchesEdit->setPlaceholderText("Optional, comma-separated (e.g. Computer Science, IT)");
        descriptionEdit = new QTextEdit;
        descriptionEdit->setMaximumHeight(90);
        descriptionEdit->setPlaceholderText("Responsibilities, team, location, and role details");
        eligibilityEdit = new QTextEdit;
        eligibilityEdit->setMaximumHeight(70);
        eligibilityEdit->setPlaceholderText("Degree, backlog, and additional eligibility requirements");
        roundsEdit = new QTextEdit;
        roundsEdit->setMaximumHeight(70);
        roundsEdit->setPlaceholderText("e.g. Aptitude, coding, technical interview, HR");
        dateEdit = new QDateEdit(QDate::currentDate().addDays(14));
        dateEdit->setCalendarPopup(true);
        dateEdit->setDisplayFormat("dd MMM yyyy");
        cgpaSpin = new QDoubleSpinBox; cgpaSpin->setRange(0, 10); cgpaSpin->setDecimals(1);
        cgpaSpin->setSingleStep(0.1); cgpaSpin->setValue(6.5);
        statusBox = new QComboBox; statusBox->addItems({ "Open", "Scheduled", "Completed" });
        form->addRow("Company", companyEdit);
        form->addRow("Job role", roleEdit);
        form->addRow("CTC package", packageEdit);
        form->addRow("Allowed branches", branchesEdit);
        form->addRow("Job description", descriptionEdit);
        form->addRow("Eligibility details", eligibilityEdit);
        form->addRow("Selection rounds", roundsEdit);
        form->addRow("Drive date", dateEdit);
        form->addRow("Minimum CGPA", cgpaSpin);
        form->addRow("Status", statusBox);

        connect(saveBtn, &QPushButton::clicked, this, [this]() {
            const QString companyError = Validator::requiredTextError(companyEdit->text(), "a company name");
            if (!companyError.isEmpty()) return fail(companyError);
            const QString roleError = Validator::requiredTextError(roleEdit->text(), "a job role");
            if (!roleError.isEmpty()) return fail(roleError);
            const QString packageError = Validator::packageError(packageEdit->text());
            if (!packageError.isEmpty()) return fail(packageError);
            const QString dateError = Validator::newDriveDateError(dateEdit->date());
            if (!dateError.isEmpty()) return fail(dateError);
            accept();
        });
    }

    Company drive() const {
        QString pkg = packageEdit->text().trimmed();
        if (!pkg.contains("LPA", Qt::CaseInsensitive)) pkg += " LPA";
        return Company(companyEdit->text().trimmed(), "", roleEdit->text().trimmed(), pkg,
                       statusBox->currentText(), dateEdit->date().toString("yyyy-MM-dd"),
                       QString::number(cgpaSpin->value(), 'f', 1), branchesEdit->text().trimmed(),
                       descriptionEdit->toPlainText().trimmed(), eligibilityEdit->toPlainText().trimmed(),
                       roundsEdit->toPlainText().trimmed());
    }
};

// ----------------------------------------------------------------------------
class EventDialog : public FormDialog {
    QLineEdit *titleEdit, *detailEdit, *companyEdit;
    QDateEdit *dateEdit;
    QTimeEdit *timeEdit;
    QComboBox *typeBox, *stateBox;

public:
    explicit EventDialog(QWidget *parent = nullptr, const QString &presetCompany = QString())
        : FormDialog("Schedule Event", "Adds an assessment or interview round to the recruitment calendar.", "Add to Calendar", parent) {
        titleEdit = new QLineEdit; titleEdit->setPlaceholderText("e.g. Infosys Online Test");
        dateEdit = new QDateEdit(QDate::currentDate().addDays(7));
        dateEdit->setCalendarPopup(true); dateEdit->setDisplayFormat("dd MMM yyyy");
        timeEdit = new QTimeEdit(QTime(10, 0)); timeEdit->setDisplayFormat("HH:mm");
        typeBox = new QComboBox; typeBox->addItems({ "Assessment", "Interview", "Completed" });
        if (!presetCompany.isEmpty()) typeBox->setCurrentText("Interview");
        stateBox = new QComboBox; stateBox->addItems({ "Scheduled", "Open", "Confirmed", "Awaiting Reply", "Completed" });
        companyEdit = new QLineEdit(presetCompany); companyEdit->setPlaceholderText("Company name (optional)");
        if (!presetCompany.isEmpty()) companyEdit->setReadOnly(true);
        detailEdit = new QLineEdit; detailEdit->setPlaceholderText("e.g. Online Coding Assessment (Lab 3)");
        form->addRow("Title", titleEdit);
        form->addRow("Date", dateEdit);
        form->addRow("Time", timeEdit);
        form->addRow("Type", typeBox);
        form->addRow("Status", stateBox);
        form->addRow("Company", companyEdit);
        form->addRow("Details", detailEdit);
        connect(saveBtn, &QPushButton::clicked, this, [this]() {
            const QString titleError = Validator::requiredTextError(titleEdit->text(), "an event title");
            if (!titleError.isEmpty()) return fail(titleError);
            accept();
        });
    }

    Event toEvent() const {
        return { dateEdit->date(), timeEdit->time().toString("HH:mm"), titleEdit->text().trimmed(),
                 typeBox->currentText(), detailEdit->text().trimmed(), companyEdit->text().trimmed(),
                 stateBox->currentText() };
    }
};

// ----------------------------------------------------------------------------
class ChangePasswordDialog : public FormDialog {
    DataStore *store;
    QString email;
    QLineEdit *currentEdit, *newEdit, *confirmEdit;

public:
    ChangePasswordDialog(DataStore *dataStore, const QString &userEmail, QWidget *parent = nullptr)
        : FormDialog("Change Password", "Choose a new password for your PlaceBridge account.",
                     "Update Password", parent), store(dataStore), email(userEmail) {
        currentEdit = new QLineEdit;
        newEdit = new QLineEdit;
        confirmEdit = new QLineEdit;
        for (QLineEdit *edit : { currentEdit, newEdit, confirmEdit })
            edit->setEchoMode(QLineEdit::Password);
        form->addRow("Current password", currentEdit);
        form->addRow("New password", newEdit);
        form->addRow("Confirm password", confirmEdit);
        connect(saveBtn, &QPushButton::clicked, this, [this]() {
            const QString passwordError = Validator::passwordError(newEdit->text());
            if (!passwordError.isEmpty()) return fail(passwordError);
            if (newEdit->text() != confirmEdit->text()) return fail("The new passwords do not match.");
            if (!store->changePassword(email, currentEdit->text(), newEdit->text()))
                return fail("Current password is incorrect or the account could not be updated.");
            accept();
        });
    }
};

class ResetPasswordDialog : public FormDialog {
    DataStore *store;
    QLineEdit *emailEdit, *newEdit, *confirmEdit;

public:
    ResetPasswordDialog(DataStore *dataStore, QWidget *parent = nullptr)
        : FormDialog("Reset Account Password", "Set a temporary password for an existing account.",
                     "Reset Password", parent), store(dataStore) {
        emailEdit = new QLineEdit;
        newEdit = new QLineEdit;
        confirmEdit = new QLineEdit;
        newEdit->setEchoMode(QLineEdit::Password);
        confirmEdit->setEchoMode(QLineEdit::Password);
        form->addRow("Account e-mail", emailEdit);
        form->addRow("New password", newEdit);
        form->addRow("Confirm password", confirmEdit);
        connect(saveBtn, &QPushButton::clicked, this, [this]() {
            const QString emailError = Validator::emailError(emailEdit->text());
            if (!emailError.isEmpty()) return fail(emailError);
            const QString passwordError = Validator::passwordError(newEdit->text());
            if (!passwordError.isEmpty()) return fail(passwordError);
            if (newEdit->text() != confirmEdit->text()) return fail("The new passwords do not match.");
            if (!store->adminResetPassword(emailEdit->text(), newEdit->text()))
                return fail("No account with this e-mail could be updated.");
            accept();
        });
    }
};

// Placement office: generate a one-time recovery code for a user.
class IssueResetCodeDialog : public FormDialog {
    DataStore *store;
    QLineEdit *emailEdit;
    QLabel *codeLabel;

public:
    IssueResetCodeDialog(DataStore *dataStore, QWidget *parent = nullptr)
        : FormDialog("Issue Password Reset Code",
                     "Generate a one-time code (valid for 15 minutes) and hand it to the user. "
                     "They enter it under \"Forgot password?\" on the sign-in screen.",
                     "Generate Code", parent), store(dataStore) {
        emailEdit = new QLineEdit;
        codeLabel = Theme::label("", "h2", nullptr, true);
        codeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        form->addRow("Account e-mail", emailEdit);
        form->addRow("One-time code", codeLabel);
        connect(saveBtn, &QPushButton::clicked, this, [this]() {
            const QString emailError = Validator::emailError(emailEdit->text());
            if (!emailError.isEmpty()) return fail(emailError);
            const QString code = store->issuePasswordResetCode(emailEdit->text());
            if (code.isEmpty()) return fail("No account with this e-mail could be found.");
            errorLabel->hide();
            codeLabel->setText(code);
            saveBtn->setText("Generate New Code");
        });
    }
};

// Sign-in screen: redeem a one-time code and choose a new password.
class RedeemResetCodeDialog : public FormDialog {
    DataStore *store;
    QLineEdit *emailEdit, *codeEdit, *newEdit, *confirmEdit;

public:
    RedeemResetCodeDialog(DataStore *dataStore, const QString &presetEmail, QWidget *parent = nullptr)
        : FormDialog("Forgot your password?",
                     "Ask the placement office for a one-time reset code, then choose a new password.",
                     "Set New Password", parent), store(dataStore) {
        emailEdit = new QLineEdit(presetEmail);
        codeEdit = new QLineEdit;
        codeEdit->setPlaceholderText("8-character code");
        newEdit = new QLineEdit;
        confirmEdit = new QLineEdit;
        newEdit->setEchoMode(QLineEdit::Password);
        confirmEdit->setEchoMode(QLineEdit::Password);
        form->addRow("Account e-mail", emailEdit);
        form->addRow("Reset code", codeEdit);
        form->addRow("New password", newEdit);
        form->addRow("Confirm password", confirmEdit);
        connect(saveBtn, &QPushButton::clicked, this, [this]() {
            const QString emailError = Validator::emailError(emailEdit->text());
            if (!emailError.isEmpty()) return fail(emailError);
            if (codeEdit->text().trimmed().isEmpty()) return fail("Enter the reset code you were given.");
            const QString passwordError = Validator::passwordError(newEdit->text());
            if (!passwordError.isEmpty()) return fail(passwordError);
            if (newEdit->text() != confirmEdit->text()) return fail("The new passwords do not match.");
            if (!store->redeemPasswordResetCode(emailEdit->text(), codeEdit->text(), newEdit->text()))
                return fail("The code is wrong, expired, or the account is temporarily locked.");
            accept();
        });
    }
};

class NotificationsDialog : public QDialog {
public:
    NotificationsDialog(DataStore *store, const QString &email, QWidget *parent = nullptr)
        : QDialog(parent) {
        setWindowTitle("Notifications");
        setMinimumSize(520, 360);
        store->markNotificationsRead(email);
        auto *layout = new QVBoxLayout(this);
        layout->addWidget(Theme::label("Notifications", "h2"));
        const auto entries = store->notifications(email);
        if (entries.isEmpty()) {
            layout->addWidget(Theme::label("You have no notifications.", "muted"));
        } else {
            auto *list = new QListWidget;
            for (const UserNotification &entry : entries) {
                const QString timestamp = entry.createdAt.toLocalTime().toString("dd MMM yyyy, HH:mm");
                list->addItem(QString("%1  ·  %2\n%3\n%4")
                                  .arg(timestamp, entry.title, entry.body, entry.type));
            }
            layout->addWidget(list, 1);
        }
        auto *close = Theme::button("Close", "primary");
        connect(close, &QPushButton::clicked, this, &QDialog::accept);
        auto *row = new QHBoxLayout;
        row->addStretch();
        row->addWidget(close);
        layout->addLayout(row);
    }
};

// ----------------------------------------------------------------------------
// Admin view of one drive: change status, see funnel numbers, delete.
class DriveManageDialog : public QDialog {
public:
    DriveManageDialog(DataStore *store, const Company &d, QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("Manage Drive");
        setModal(true);
        setMinimumWidth(460);
        auto *v = new QVBoxLayout(this);
        v->setContentsMargins(28, 24, 28, 22);
        v->setSpacing(14);

        auto *top = new QHBoxLayout;
        top->setSpacing(14);
        top->addWidget(Theme::logo(d.initials(), false, 52));
        auto *names = new QVBoxLayout;
        names->setSpacing(0);
        names->addWidget(Theme::label(d.getName(), "h2"));
        names->addWidget(Theme::label(d.getJobRole(), "muted"));
        top->addLayout(names, 1);
        top->addWidget(Theme::statusPill(d.getDriveStatus()), 0, Qt::AlignTop);
        v->addLayout(top);

        int applicants = 0, shortlisted = 0, offers = 0;
        for (const Application &a : store->applications()) {
            if (a.company != d.getName() || a.role != d.getJobRole()) continue;
            ++applicants;
            if (a.stage() >= 1) ++shortlisted;
            if (a.stage() >= 3) ++offers;
        }
        auto *info = new QFrame; info->setObjectName("softCard");
        auto *iv = Theme::vbox(info, 16, 6);
        iv->addWidget(Theme::label(QString("CTC package: %1    ·    Drive date: %2    ·    Min CGPA: %3")
                                       .arg(d.getPackageOffer(), Fmt::dateLong(d.date()), d.getMinCgpa()), "muted", nullptr, true));
        iv->addWidget(Theme::label(QString("%1 applicants  ·  %2 shortlisted  ·  %3 offers")
                                       .arg(applicants).arg(shortlisted).arg(offers), "h3"));
        v->addWidget(info);

        auto *status = new QComboBox;
        status->addItems({ "Open", "Scheduled", "Completed" });
        status->setCurrentText(d.getDriveStatus());
        auto *form = new QFormLayout;
        form->addRow("Drive status", status);
        v->addLayout(form);

        auto *row = new QHBoxLayout;
        auto *del = Theme::button("Delete drive", "danger");
        auto *close = Theme::button("Close", "soft");
        auto *save = Theme::button("Save status", "primary");
        row->addWidget(del);
        row->addStretch();
        row->addWidget(close);
        row->addWidget(save);
        v->addLayout(row);

        const QString company = d.getName(), role = d.getJobRole();
        connect(close, &QPushButton::clicked, this, &QDialog::reject);
        connect(save, &QPushButton::clicked, this, [=]() {
            store->setDriveStatus(company, role, status->currentText());
            accept();
        });
        connect(del, &QPushButton::clicked, this, [=]() {
            if (QMessageBox::question(this, "Delete drive",
                    QString("Permanently remove the %1 – %2 drive?").arg(company, role)) == QMessageBox::Yes) {
                store->removeDrive(company, role);
                accept();
            }
        });
    }
};

// ----------------------------------------------------------------------------
// Read-only candidate profile (used by "View Profile" / "View Resume").
class StudentInfoDialog : public QDialog {
public:
    StudentInfoDialog(const Student &s, const QVector<Application> &allApps, QWidget *parent = nullptr)
        : QDialog(parent) {
        setWindowTitle(s.getName());
        setMinimumWidth(500);
        auto *v = new QVBoxLayout(this);
        v->setContentsMargins(28, 24, 28, 22);
        v->setSpacing(14);

        auto *top = new QHBoxLayout;
        top->setSpacing(16);
        top->addWidget(Theme::logo(s.initials(), true, 64));
        auto *nm = new QVBoxLayout;
        nm->setSpacing(2);
        nm->addWidget(Theme::label(s.getName(), "bigName"));
        nm->addWidget(Theme::label(s.getCourse() + (s.getRollNo().isEmpty() ? "" : "  ·  Roll No " + s.getRollNo()), "muted"));
        top->addLayout(nm, 1);
        top->addWidget(Theme::statusPill(s.getStatus()), 0, Qt::AlignTop);
        v->addLayout(top);

        auto *grid = new QGridLayout;
        grid->setHorizontalSpacing(30);
        grid->setVerticalSpacing(10);
        auto fact = [&](int r, int c, const QString &k, const QString &val) {
            auto *box = new QVBoxLayout;
            box->setSpacing(2);
            box->addWidget(Theme::label(k, "eyebrow"));
            box->addWidget(Theme::label(val, "value", nullptr, true));
            grid->addLayout(box, r, c);
        };
        fact(0, 0, "CUMULATIVE CGPA", s.cgpaValue() >= 0 ? s.getCgpa() + " / 10.0" : "Not recorded");
        fact(0, 1, "ONLINE TEST", s.testValue() >= 0 ? QString::number(s.testValue()) + " / 100" : "Not taken");
        fact(1, 0, "CAMPUS E-MAIL", s.getEmail());
        fact(1, 1, "STATUS", s.getStatus());
        v->addLayout(grid);

        if (!s.skillList().isEmpty()) {
            v->addWidget(Theme::label("SKILLS", "eyebrow"));
            auto *sk = new QHBoxLayout;
            sk->setSpacing(6);
            for (const QString &x : s.skillList()) sk->addWidget(Theme::label(x, "skill"));
            sk->addStretch();
            v->addLayout(sk);
        }

        v->addWidget(Theme::label("APPLICATIONS", "eyebrow"));
        bool any = false;
        for (const Application &a : allApps) {
            if (a.student != s.getName()) continue;
            any = true;
            auto *row = new QHBoxLayout;
            row->addWidget(Theme::label(a.company + " — " + a.role, "h3"), 1);
            row->addWidget(Theme::statusPill(a.status));
            v->addLayout(row);
        }
        if (!any) v->addWidget(Theme::label("No applications yet.", "muted"));

        auto *close = Theme::button("Close", "primary");
        connect(close, &QPushButton::clicked, this, &QDialog::accept);
        auto *row = new QHBoxLayout;
        row->addStretch();
        row->addWidget(close);
        v->addLayout(row);
    }
};

#endif // DIALOGS_H
