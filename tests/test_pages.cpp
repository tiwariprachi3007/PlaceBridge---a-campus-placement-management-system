// Smoke test for the redesigned UI: builds every page for every role against a
// seeded in-memory store and exercises the main placement workflows end to end.
// A page that crashes, denies a role that should see it, or renders empty is the
// failure mode a pure data-layer test cannot catch.
#include "theme.h"
#include "context.h"
#include "dialogs.h"
#include "search_dialog.h"
#include "login_page.h"
#include "dashboard_page.h"
#include "drives_page.h"
#include "students_page.h"
#include "calendar_page.h"
#include "reports_page.h"
#include "student_portal.h"
#include "recruiter_page.h"
#include "applications_page.h"
#include "activity_page.h"
#include "settings_page.h"
#include "resume_studio.h"
#include "placement_officer.h"

#include <QTemporaryDir>
#include <QtTest>

namespace {

const char *kAdmin = "officer@placebridge.edu";
const char *kStudent = "rahul.sharma@placebridge.edu";
const char *kRecruiter = "recruiter@accenture.com";

// Any page is a QWidget; show it at a realistic size and let it lay out.
void realize(QWidget *page, int width = 1280, int height = 820) {
    page->resize(width, height);
    page->show();
    QCoreApplication::processEvents();
}

int descendants(QWidget *page) { return page->findChildren<QWidget *>().size(); }

} // namespace

class PageTests : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { Theme::install(*qApp); }

    // ---------------------------------------------------------------- theme
    void themeTokensCoverBothModes() {
        for (const QString &mode : { "Light", "Dark" }) {
            QSettings().setValue("theme/mode", mode);
            Theme::invalidateTokens();
            const Theme::Tokens t = Theme::tokens();
            for (const QString &value : { t.background, t.surface, t.surfaceAlt, t.border, t.text,
                                          t.textMuted, t.accent, t.accentSoft, t.accentText,
                                          t.sidebar, t.sidebarText, t.success, t.warning, t.danger })
                QVERIFY2(QColor(value).isValid(), qPrintable(mode + " token: " + value));
            // text must stay readable against the surface it sits on
            QVERIFY(qAbs(QColor(t.text).lightness() - QColor(t.surface).lightness()) > 80);
            const QString sheet = Theme::stylesheet();
            QVERIFY(sheet.size() > 2000);
            QVERIFY2(!sheet.contains('@'), "every @TOKEN@ placeholder must be substituted");
        }
        QSettings().setValue("theme/mode", "Light");
        Theme::invalidateTokens();
    }

    // ------------------------------------------------------- admin's pages
    void adminPagesBuild() {
        DataStore store(":memory:");
        store.setActor(kAdmin);
        PlacementOfficer officer("Prof. Rajesh Verma", kAdmin, "PO-2024");
        Ctx ctx{ &store, &officer, [](const QString &) {}, [](bool) {} };

        QScopedPointer<DashboardPage> dashboard(new DashboardPage(ctx));
        realize(dashboard.data());
        QVERIFY(descendants(dashboard.data()) > 40);

        QScopedPointer<DrivesPage> drives(new DrivesPage(ctx));
        realize(drives.data());
        QVERIFY(descendants(drives.data()) > 40);

        QScopedPointer<CompaniesPage> companies(new CompaniesPage(ctx));
        realize(companies.data());
        QVERIFY(descendants(companies.data()) > 20);

        QScopedPointer<StudentsPage> students(new StudentsPage(ctx));
        realize(students.data());
        auto *registry = students->findChild<QTableWidget *>();
        QVERIFY(registry);
        QVERIFY2(registry->rowCount() > 0, "the seeded candidates must reach the table");

        QScopedPointer<CalendarPage> calendar(new CalendarPage(ctx));
        realize(calendar.data());
        QVERIFY(descendants(calendar.data()) > 40);

        QScopedPointer<ReportsPage> reports(new ReportsPage(ctx));
        realize(reports.data());
        // BarChart is a plain QWidget (no Q_OBJECT), so find it by accessible name.
        bool charted = false;
        for (QWidget *child : reports->findChildren<QWidget *>())
            charted |= child->accessibleName() == "Bar chart";
        QVERIFY2(charted, "the placed-by-branch chart must render");

        QScopedPointer<ApplicationsPage> applications(new ApplicationsPage(ctx));
        realize(applications.data());
        auto *pipeline = applications->findChild<QTableWidget *>();
        QVERIFY(pipeline && pipeline->rowCount() > 0);

        QScopedPointer<ActivityPage> activity(new ActivityPage(ctx));
        realize(activity.data());
        QVERIFY(activity->findChild<QTableWidget *>());

        QScopedPointer<SettingsPage> settings(new SettingsPage(ctx));
        realize(settings.data());
        QVERIFY(descendants(settings.data()) > 20);
    }

    void coordinatorSeesOnlyItsBranch() {
        DataStore store(":memory:");
        store.setActor(kAdmin);
        QVERIFY(store.registerAccount("cs.coord@placebridge.edu", "coordinator-pass",
                                      "Department Coordinator", "CS Coordinator",
                                      "Computer Science").isEmpty());
        store.setActor("cs.coord@placebridge.edu");
        DepartmentCoordinator coordinator("CS Coordinator", "cs.coord@placebridge.edu",
                                          "Computer Science");
        Ctx ctx{ &store, &coordinator, [](const QString &) {}, [](bool) {} };

        QScopedPointer<StudentsPage> students(new StudentsPage(ctx));
        realize(students.data());
        auto *table = students->findChild<QTableWidget *>();
        QVERIFY(table);
        QVERIFY(table->rowCount() > 0);
        for (const Student &s : store.students())
            QVERIFY2(s.getCourse().contains("Computer Science"),
                     qPrintable("coordinator saw " + s.getCourse()));

        QScopedPointer<DashboardPage> dashboard(new DashboardPage(ctx));
        realize(dashboard.data());
        QVERIFY(descendants(dashboard.data()) > 20);
    }

    // ----------------------------------------------------- student's pages
    void studentPagesAndApplyFlow() {
        DataStore store(":memory:");
        store.setActor(kStudent);
        const auto record = store.findStudent(kStudent);
        QVERIFY(record);
        Student student(*record);
        Ctx ctx{ &store, &student, [](const QString &) {}, [](bool) {} };

        QScopedPointer<StudentPortalPage> portal(new StudentPortalPage(ctx));
        realize(portal.data());
        QVERIFY(descendants(portal.data()) > 40);

        QScopedPointer<DrivesPage> drives(new DrivesPage(ctx));
        realize(drives.data());
        QVERIFY(descendants(drives.data()) > 30);

        QScopedPointer<CalendarPage> calendar(new CalendarPage(ctx));
        realize(calendar.data());

        // a student must not reach the admin registry
        QScopedPointer<StudentsPage> students(new StudentsPage(ctx));
        realize(students.data());
        auto *table = students->findChild<QTableWidget *>();
        QVERIFY(table);
        QCOMPARE(table->rowCount(), 1);            // only their own record

        // batched eligibility must agree with the single-drive check
        const auto all = store.eligibilityForVisibleDrives(kStudent);
        QVERIFY(!all.isEmpty());
        for (const Company &drive : store.drives()) {
            const QString key = Application::driveKey(drive.getName(), drive.getJobRole());
            QCOMPARE(all.value(key),
                     store.eligibilityForDrive(kStudent, drive.getName(), drive.getJobRole()));
        }

        // apply to the first drive this student is eligible for, then withdraw
        QString company, role;
        for (const Company &drive : store.drives()) {
            if (drive.getDriveStatus() != "Open") continue;
            const QString key = Application::driveKey(drive.getName(), drive.getJobRole());
            if (!all.value(key).isEmpty()) continue;
            if (store.appliedDriveKeys(student.getName()).contains(key)) continue;
            company = drive.getName();
            role = drive.getJobRole();
            break;
        }
        if (!company.isEmpty()) {
            QVERIFY(store.addApplication({ student.getName(), company, role, "Applied" }));
            QVERIFY(store.appliedDriveKeys(student.getName())
                        .contains(Application::driveKey(company, role)));
            QVERIFY(store.withdrawApplication(student.getName(), company, role));
        }
    }

    void studentAcceptsAnOffer() {
        DataStore store(":memory:");
        store.setActor(kStudent);
        const auto record = store.findStudent(kStudent);
        QVERIFY(record);
        // the seed gives this student an Infosys offer
        QVERIFY(store.acceptOffer(kStudent, "Infosys", "Systems Engineer Specialist"));
        const auto after = store.findStudent(kStudent);
        QVERIFY(after);
        QCOMPARE(after->getStatus(), QString("Placed"));
        // a second acceptance must be refused by the one-offer policy
        QVERIFY(!store.acceptOffer(kStudent, "TCS Digital", "Associate Software Engineer"));
    }

    // ---------------------------------------------------- recruiter's pages
    void recruiterPageAndStageChange() {
        DataStore store(":memory:");
        store.setActor(kRecruiter);
        Company recruiter("Accenture", kRecruiter, "", "", "Open");
        Ctx ctx{ &store, &recruiter, [](const QString &) {}, [](bool) {} };

        QScopedPointer<RecruiterPage> suite(new RecruiterPage(ctx));
        realize(suite.data());
        QVERIFY(descendants(suite.data()) > 40);
        auto *roster = suite->findChild<QTableWidget *>();
        QVERIFY(roster);

        QScopedPointer<CalendarPage> calendar(new CalendarPage(ctx));
        realize(calendar.data());

        // recruiters move their own applications and only their own
        QVERIFY(store.updateApplicationStatus("Rahul Sharma", "Accenture",
                                              "Technology Consultant", "Shortlisted"));
        QVERIFY(!store.updateApplicationStatus("Sneha Patel", "Google",
                                               "Software Engineer", "Rejected"));
    }

    // ------------------------------------------------------- resume studio
    void resumeStudioScoresAndRenders() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        DataStore store(":memory:");
        store.setActor(kStudent);
        const auto record = store.findStudent(kStudent);
        QVERIFY(record);
        Student student(*record);
        Ctx ctx{ &store, &student, [](const QString &) {}, [](bool) {} };

        QScopedPointer<ResumeStudioPage> studio(new ResumeStudioPage(ctx, directory.path()));
        realize(studio.data(), 1360, 820);
        auto *score = studio->findChild<QLabel *>("resumeScore");
        auto *bar = studio->findChild<QProgressBar *>("resumeScoreBar");
        auto *hint = studio->findChild<QLabel *>("resumeScoreHint");
        QVERIFY(score && bar && hint);
        QVERIFY2(score->text().contains('%'), qPrintable(score->text()));
        QVERIFY(bar->value() >= 0 && bar->value() <= 100);

        // an empty resume scores low and names something to fix
        ResumeData empty;
        const ResumeScore low = ResumeScorer::evaluate(empty);
        QVERIFY(low.percent < 20);
        QVERIFY(!low.missing.isEmpty());

        // a complete one scores full and has nothing left to say
        ResumeData full;
        full.fullName = "Jordan Student";
        full.email = "jordan@example.edu";
        full.phone = "+91 90000 00000";
        full.location = "Pune";
        full.github = "https://github.com/jordan";
        full.objective = "Final-year computer science student looking for a backend internship.";
        full.technicalSkills = "C++, Qt, SQL, Python";
        full.education.append({ "Example University", "B.Tech", "Computer Science", "2027", "8.7", {}, {}, {} });
        full.projects.append({ "Study Planner", {}, {}, {}, {}, "Plans revision schedules.", "Qt", {} });
        full.certifications.append({ "Azure Fundamentals", {}, {}, {}, {}, {}, {}, {} });
        full.achievements.append({ "Hackathon winner", {}, {}, {}, {}, {}, {}, {} });
        const ResumeScore high = ResumeScorer::evaluate(full);
        QCOMPARE(high.percent, 100);
        QVERIFY(high.missing.isEmpty());

        // every template renders, escapes input and stays distinct
        QString previous;
        for (const QString &name : { "Modern", "Classic", "Minimal" }) {
            full.templateName = name;
            const QString html = ResumeDocumentRenderer::html(full);
            QVERIFY2(html.contains("Jordan Student"), qPrintable(name));
            QVERIFY2(html.contains("Study Planner"), qPrintable(name));
            QVERIFY2(!html.contains("<script>"), qPrintable(name));
            QVERIFY2(html != previous, qPrintable(name + " must differ from the previous template"));
            previous = html;
        }

        const QString pdf = directory.filePath("resume.pdf");
        QCOMPARE(ResumePdfExporter::exportFile(pdf, full), QString());
        QFile written(pdf);
        QVERIFY(written.open(QIODevice::ReadOnly));
        QCOMPARE(written.read(5), QByteArray("%PDF-"));
    }

    // -------------------------------------------------------------- search
    void searchIsPermissionAware() {
        DataStore store(":memory:");
        store.setActor(kAdmin);
        SearchDialog adminSearch(&store);
        auto *input = adminSearch.findChild<QLineEdit *>();
        QVERIFY(input);
        input->setText("Sneha");
        QTRY_VERIFY(adminSearch.findChild<QListWidget *>()->count() > 0);

        store.setActor(kStudent);
        const auto hits = GlobalSearch::run(store, "Sneha");
        for (const SearchHit &hit : hits)
            QVERIFY2(hit.kind != "Student", "a student must not find other students");
    }

    // --------------------------------------------------------- login screen
    void loginScreenSignsInEveryRole() {
        DataStore store(":memory:");
        LoginPage page(&store);
        realize(&page, 1280, 860);
        QTest::mouseClick(page.findChild<QPushButton *>("landingLogin"), Qt::LeftButton);

        auto *email = page.findChild<QLineEdit *>("loginEmail");
        auto *password = page.findChild<QLineEdit *>("loginPassword");
        auto *submit = page.findChild<QPushButton *>("loginSubmit");
        QVERIFY(email && password && submit);
        QSignalSpy authenticated(&page, &LoginPage::authenticated);

        struct Account { const char *role; const char *email; const char *password; };
        const QVector<Account> accounts = {
            { "role_student", kStudent, "student123" },
            { "role_recruiter", kRecruiter, "recruit123" },
            { "role_admin", kAdmin, "admin123" }
        };
        for (const Account &account : accounts) {
            auto *roleButton = page.findChild<QPushButton *>(account.role);
            QVERIFY2(roleButton, account.role);
            QTest::mouseClick(roleButton, Qt::LeftButton);
            email->setText(QString::fromLatin1(account.email));
            password->setText(QString::fromLatin1(account.password));
            QTest::mouseClick(submit, Qt::LeftButton);
        }
        QCOMPARE(authenticated.count(), accounts.size());
    }
};

QTEST_MAIN(PageTests)
#include "test_pages.moc"
