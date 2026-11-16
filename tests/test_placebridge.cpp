#include "student.h"
#include "company.h"
#include "SecurityManager.h"
#include "DataManager.h"
#include "SystemServices.h"
#include "placement_officer.h"

#include <QtTest>

class PlaceBridgeTests : public QObject {
    Q_OBJECT

private slots:
    void studentDomain() {
        const Student student("Alex Doe", "alex@example.edu", "B.Tech Computer Science", "8.5",
                              "Registered", "24CS001", "C++, SQL", "90");
        QCOMPARE(student.role(), QString("Student"));
        QCOMPARE(student.branch(), QString("Computer Science"));
        QCOMPARE(student.cgpaValue(), 8.5);
        QCOMPARE(student.skillList(), QStringList({ "C++", "SQL" }));
    }

    void companyDomain() {
        Company company("Example", "", "Developer", "12 LPA", "Open", "2027-01-15", "7.0",
                        "Computer Science, IT");
        QVERIFY(company.allowsBranch("computer science"));
        QVERIFY(company.allowsBranch("IT"));
        QVERIFY(!company.allowsBranch("Mechanical"));
        company.setAllowedBranches({});
        QVERIFY(company.allowsBranch("any branch"));
    }

    void validatorRules() {
        QVERIFY(Validator::emailError("person@example.edu").isEmpty());
        QVERIFY(!Validator::emailError("not-an-email").isEmpty());
        QVERIFY(Validator::cgpaError("10").isEmpty());
        QVERIFY(!Validator::cgpaError("10.1").isEmpty());
        QVERIFY(Validator::packageError("12.5 LPA").isEmpty());
        QVERIFY(!Validator::packageError("negotiable").isEmpty());
        QVERIFY(Validator::newDriveDateError(QDate::currentDate().addDays(1)).isEmpty());
        QVERIFY(!Validator::newDriveDateError(QDate::currentDate().addDays(-1)).isEmpty());
    }

    void placementPolicyCategories() {
        QCOMPARE(PlacementPolicy::categoryFor(7.99), PlacementPolicy::Category::Regular);
        QCOMPARE(PlacementPolicy::categoryFor(8.0), PlacementPolicy::Category::Dream);
        QCOMPARE(PlacementPolicy::categoryFor(12.0), PlacementPolicy::Category::Dream);
        QCOMPARE(PlacementPolicy::categoryFor(12.01), PlacementPolicy::Category::SuperDream);
        const Student student("Alex Doe", "alex@example.edu", "B.Tech Computer Science",
                              "7.0", "Registered", "24CS001", "", "");
        const Company drive("Example", "", "Developer", "12 LPA", "Open",
                             "2027-01-15", "8.0", "Computer Science");
        QVERIFY(!PlacementPolicy::eligibilityReason(student, drive, 0, 2026, 0).isEmpty());
        QVERIFY(!PlacementPolicy::eligibilityReason(student, drive, 1, 2026, 0).isEmpty());
        QVERIFY(!PlacementPolicy::eligibilityReason(student, drive, 0, 2026, 2).isEmpty());
    }

    // ---- permissions: enum + per-object (polymorphic) capabilities ---------------
    void permissionsAreConsistentAcrossStringsAndObjects() {
        using Role = Permissions::Role;
        QCOMPARE(Permissions::fromName("Admin"), Role::Admin);
        QCOMPARE(Permissions::fromName("Placement Officer"), Role::Admin);   // display name of PlacementOfficer
        QCOMPARE(Permissions::fromName("Company"), Role::Recruiter);         // display name of Company
        QCOMPARE(Permissions::fromName("Department Coordinator"), Role::Coordinator);
        QCOMPARE(Permissions::fromName("nonsense"), Role::None);
        QVERIFY(Permissions::isAdmin("Placement Officer"));
        QVERIFY(!Permissions::canManageDrives("Student"));
        QVERIFY(!Permissions::canManageDrives(""));

        const PlacementOfficer officer("Officer", "o@example.edu", "PO-1");
        const DepartmentCoordinator coordinator("Coord", "c@example.edu", "Computer Science");
        const Company recruiter("Acme", "r@acme.com", "Dev", "10 LPA");
        const Student student("Stu", "s@example.edu", "B.Tech Computer Science");
        const QVector<const User *> everyone = { &officer, &coordinator, &recruiter, &student };
        for (const User *user : everyone)   // object answer must equal string answer
            QCOMPARE(user->canManageDrives(), Permissions::canManageDrives(user->role()));

        QVERIFY(officer.canManageStudents() && officer.canViewReports() && officer.canViewActivity());
        QVERIFY(recruiter.canManageDrives() && recruiter.canManageApplications());
        QVERIFY(!recruiter.canManageStudents() && !recruiter.canViewReports());
        QVERIFY(coordinator.canReadAllStudents() && !coordinator.canManageDrives());
        QVERIFY(!student.canManageStudents() && !student.canManageApplications() && !student.canReadAllStudents());
    }

    // ---- eligibility boundaries -----------------------------------------------------
    void eligibilityBoundaries() {
        const QDate today(2026, 6, 1);
        const Student student("Alex Doe", "alex@example.edu", "B.Tech Computer Science",
                              "8.0", "Registered", "24CS001", "", "");
        const Company atCutoff("Example", "", "Developer", "9 LPA", "Open", "2027-01-15", "8.0", "Computer Science");
        const Company aboveCutoff("Example", "", "Developer", "9 LPA", "Open", "2027-01-15", "8.1", "Computer Science");
        QVERIFY(PlacementPolicy::eligibilityReason(student, atCutoff, 0, 2026, 0, 2, today).isEmpty());
        QVERIFY(!PlacementPolicy::eligibilityReason(student, aboveCutoff, 0, 2026, 0, 2, today).isEmpty());
        // offers: one below the limit is fine, at the limit is blocked
        QVERIFY(PlacementPolicy::eligibilityReason(student, atCutoff, 0, 2026, 1, 2, today).isEmpty());
        QVERIFY(!PlacementPolicy::eligibilityReason(student, atCutoff, 0, 2026, 2, 2, today).isEmpty());
        // batch window: 4 years old is fine, 5 is not
        QVERIFY(PlacementPolicy::eligibilityReason(student, atCutoff, 0, 2022, 0, 2, today).isEmpty());
        QVERIFY(!PlacementPolicy::eligibilityReason(student, atCutoff, 0, 2021, 0, 2, today).isEmpty());
        // exactly 12 LPA is Dream, not Super Dream; 8 LPA is the Dream boundary
        QCOMPARE(PlacementPolicy::categoryFor(Util::parseLpa("12 LPA")), PlacementPolicy::Category::Dream);
        QCOMPARE(PlacementPolicy::categoryFor(Util::parseLpa("8 LPA")), PlacementPolicy::Category::Dream);
        // after accepting a Super Dream offer, lower categories are locked but Super Dream is not
        const Company regular("Small", "", "Dev", "5 LPA", "Open", "2027-01-15", "", "");
        const Company superDream("Big", "", "Dev", "20 LPA", "Open", "2027-01-15", "", "");
        QVERIFY(!PlacementPolicy::eligibilityReason(student, regular, 0, 2026, 1, 2, today, 2).isEmpty());
        QVERIFY(PlacementPolicy::eligibilityReason(student, superDream, 0, 2026, 1, 2, today, 2).isEmpty());
    }

    // ---- analytics ----------------------------------------------------------------------
    void placementStatistics() {
        const QVector<Student> students = {
            Student("A", "a@x.edu", "B.Tech Computer Science", "8", "Placed"),
            Student("B", "b@x.edu", "B.Tech Computer Science", "7", "Registered"),
            Student("C", "c@x.edu", "B.Tech Information Technology", "9", "Selected"),
            Student("D", "d@x.edu", "B.Tech Electronics & Comm", "6", "Registered")
        };
        const QVector<BranchStat> branches = PlacementStats::byBranch(students);
        QCOMPARE(branches.size(), 3);
        QCOMPARE(branches[0].branch, QString("Computer Science"));   // alphabetical
        QCOMPARE(branches[0].total, 2);
        QCOMPARE(branches[0].placed, 1);
        QCOMPARE(branches[0].percent(), 50);
        QCOMPARE(PlacementStats::placementRatePercent(students), 50);
        QCOMPARE(PlacementStats::placementRatePercent({}), 0);
        QCOMPARE(PlacementStats::chartLabel("Computer Science"), QString("CS"));
        QCOMPARE(PlacementStats::chartLabel("Electronics & Comm"), QString("EC"));
        QCOMPARE(PlacementStats::chartLabel("Civil"), QString("Civil"));
        QCOMPARE(PlacementStats::placedByBranchChart(students).size(), 3);

        const QVector<Company> drives = {
            Company("One", "", "Dev", "10 LPA"), Company("Two", "", "Dev", "20 LPA"),
            Company("Three", "", "Dev", "TBD")
        };
        QCOMPARE(PlacementStats::averagePackage(drives), 15.0);
        QCOMPARE(PlacementStats::averagePackage({}), 0.0);

        const QVector<Application> applications = {
            { "A", "Acme", "Dev", "Offer" }, { "B", "Acme", "Dev", "Offer" },
            { "C", "Beta", "Dev", "Offer" }, { "D", "Beta", "Dev", "Rejected" },
            { "E", "Gamma", "Dev", "Applied" }
        };
        const auto offers = PlacementStats::offersByCompany(applications);
        QCOMPARE(offers.size(), 2);
        QCOMPARE(offers[0].first, QString("Acme"));
        QCOMPARE(offers[0].second, 2);
    }

    // ---- password recovery, demo-password nudge ---------------------------------------
    void resetCodeFlow() {
        DataStore store(":memory:");
        const QString student = "rahul.sharma@placebridge.edu";
        store.setActor(student);
        QVERIFY(store.issuePasswordResetCode(student).isEmpty());          // not an admin
        store.setActor("officer@placebridge.edu");
        QVERIFY(store.issuePasswordResetCode("nobody@example.edu").isEmpty());
        const QString code = store.issuePasswordResetCode(student);
        QCOMPARE(code.size(), 8);
        store.setActor({});
        QVERIFY(!store.redeemPasswordResetCode(student, "WRONGCODE", "Brand-new-pass-1"));
        QVERIFY(store.redeemPasswordResetCode(student, code.toLower(), "Brand-new-pass-1"));   // case-insensitive
        QVERIFY(store.authenticate(student, "Brand-new-pass-1").has_value());
        QVERIFY(!store.authenticate(student, "student123").has_value());
        QVERIFY(!store.redeemPasswordResetCode(student, code, "Another-pass-2"));              // single use
    }

    void wrongResetCodesTriggerLockout() {
        DataStore store(":memory:");
        const QString student = "rahul.sharma@placebridge.edu";
        store.setActor("officer@placebridge.edu");
        const QString code = store.issuePasswordResetCode(student);
        store.setActor({});
        for (int attempt = 0; attempt < 5; ++attempt)
            QVERIFY(!store.redeemPasswordResetCode(student, "AAAAAAAA", "Brand-new-pass-1"));
        QVERIFY(store.loginLockoutSeconds(student) > 0);
        QVERIFY(!store.redeemPasswordResetCode(student, code, "Brand-new-pass-1"));   // locked out
    }

    void demoPasswordDetection() {
        DataStore store(":memory:");
        const QString student = "rahul.sharma@placebridge.edu";
        store.setActor(student);
        QVERIFY(store.usesDefaultPassword(student));
        QVERIFY(!store.usesDefaultPassword("officer@placebridge.edu"));   // only answers for own account
        QVERIFY(store.changePassword(student, "student123", "Better-pass-1"));
        QVERIFY(!store.usesDefaultPassword(student));
        store.setActor({});
        QVERIFY(!store.usesDefaultPassword(student));
    }

    // ---- reminders --------------------------------------------------------------------
    void remindersAreSentOnceAndOnlyByAdmins() {
        DataStore store(":memory:");
        store.setActor("rahul.sharma@placebridge.edu");
        QCOMPARE(store.sendUpcomingReminders(2), 0);                          // students may not send
        store.setActor("officer@placebridge.edu");
        const QDate tomorrow = QDate::currentDate().addDays(1);
        QVERIFY(store.addDrive(Company("Remind Corp", "", "Analyst", "10 LPA", "Open",
                                       tomorrow.toString("yyyy-MM-dd"), "6.0")));
        QVERIFY(store.addApplication({ "Rahul Sharma", "Remind Corp", "Analyst", "Applied" }));
        QVERIFY(store.sendUpcomingReminders(2) >= 1);
        QCOMPARE(store.sendUpcomingReminders(2), 0);                          // idempotent
        store.setActor("rahul.sharma@placebridge.edu");
        bool found = false;
        for (const UserNotification &note : store.notifications("rahul.sharma@placebridge.edu"))
            found |= note.title.startsWith("Reminder: Remind Corp");
        QVERIFY(found);
    }

    // ---- password hashing -------------------------------------------------------------
    void passwordHasherProperties() {
        const QByteArray salt = QByteArray::fromHex(PasswordHasher::randomSalt());
        QCOMPARE(salt.size(), 16);
        const QByteArray one = PasswordHasher::pbkdf2("secret", salt);
        QCOMPARE(one.size(), 64);                                        // SHA-256 digest, hex
        QCOMPARE(PasswordHasher::pbkdf2("secret", salt), one);           // deterministic
        QVERIFY(PasswordHasher::pbkdf2("Secret", salt) != one);          // password-sensitive
        QVERIFY(PasswordHasher::pbkdf2("secret", QByteArray::fromHex(PasswordHasher::randomSalt())) != one);
        QVERIFY(PasswordHasher::constantTimeEqual(one, one));
        QVERIFY(!PasswordHasher::constantTimeEqual(one, one.left(10)));
        QVERIFY(!PasswordHasher::constantTimeEqual("abc", "abd"));
    }

    // ---- global search respects permissions ------------------------------------------
    void globalSearchRespectsPermissions() {
        DataStore store(":memory:");
        store.setActor("officer@placebridge.edu");
        QVERIFY(GlobalSearch::run(store, "").isEmpty());
        bool officerSeesSneha = false;
        for (const SearchHit &hit : GlobalSearch::run(store, "sneha patel"))
            officerSeesSneha |= hit.kind == "Student" && hit.title == "Sneha Patel";
        QVERIFY(officerSeesSneha);

        store.setActor("rahul.sharma@placebridge.edu");
        for (const SearchHit &hit : GlobalSearch::run(store, "sneha"))
            QVERIFY(hit.kind != "Student");          // a student cannot find other students
        bool ownRecord = false;
        for (const SearchHit &hit : GlobalSearch::run(store, "rahul"))
            ownRecord |= hit.kind == "Student";
        QVERIFY(ownRecord);
    }

    void sqliteStoreWorkflow() {
        DataStore store(":memory:");
        store.setActor("officer@placebridge.edu");
        QVERIFY(store.students().size() >= 1);
        QVERIFY(store.findAccount("officer@placebridge.edu").has_value());
        QVERIFY(store.authenticate("officer@placebridge.edu", "admin123").has_value());
        store.setActor({});
        QVERIFY(!store.findAccount("officer@placebridge.edu").has_value());
        QVERIFY(store.authenticate("officer@placebridge.edu", "admin123").has_value());
        store.setActor("officer@placebridge.edu");
        QVERIFY(!store.authenticate("officer@placebridge.edu", "wrong-password").has_value());
        QVERIFY(store.changePassword("officer@placebridge.edu", "admin123", "new-admin-pass"));
        QVERIFY(store.authenticate("officer@placebridge.edu", "new-admin-pass").has_value());
        QCOMPARE(store.registerAccount("coordinator@example.edu", "coordinator-pass",
            "Department Coordinator", "Coordinator", "Information Technology"), QString());
        const auto coordinator = store.authenticate("coordinator@example.edu", "coordinator-pass");
        QVERIFY(coordinator.has_value());
        QCOMPARE(coordinator->role, QString("Department Coordinator"));
        QCOMPARE(store.registerAccount("admin2@example.edu", "new-admin-pass",
            "Admin", "Second Admin", "PO-2025"), QString());
        const auto secondAdmin = store.authenticate("admin2@example.edu", "new-admin-pass");
        QVERIFY(secondAdmin.has_value());
        QCOMPARE(secondAdmin->role, QString("Admin"));
        QCOMPARE(store.registerAccount("newstudent@example.edu", "new-student-pass",
            "Student", "New Student", "B.Tech Mechanical"), QString());
        QVERIFY(store.addStudent(Student("New Student", "newstudent@example.edu", "B.Tech Mechanical")));
        QVERIFY(store.findStudent("newstudent@example.edu").has_value());
        store.setActor("coordinator@example.edu");
        QVERIFY(store.postAnnouncement("Information Technology", "Branch placement briefing"));
        QVERIFY(!store.postAnnouncement("Computer Science", "Out-of-scope notice"));
        QVERIFY(!store.findStudent("rahul.sharma@placebridge.edu").has_value());
        store.setActor("officer@placebridge.edu");
        store.clearFailedLogins("recruiter@accenture.com");
        for (int attempt = 0; attempt < 5; ++attempt)
            store.recordFailedLogin("recruiter@accenture.com");
        QVERIFY(store.loginLockoutSeconds("recruiter@accenture.com") > 0);
        QVERIFY(!store.authenticate("recruiter@accenture.com", "recruit123").has_value());
        store.clearFailedLogins("recruiter@accenture.com");

        const Student added("Test User", "test.user@example.edu", "B.Tech Information Technology",
                            "8.0", "Registered", "TEST001", "", "");
        QVERIFY(store.addStudent(added));
        QVERIFY(!store.addStudent(added));
        QVERIFY(store.hasStudentRollNumber("test001"));
        QVERIFY(!store.addStudent(Student("Another User", "another@example.edu",
                                          "B.Tech Computer Science", "7.0", "Registered",
                                          "TEST001", "", "")));
        store.setActor("coordinator@example.edu");
        const QVector<Student> branchStudents = store.students();
        QVERIFY(!branchStudents.isEmpty());
        for (const Student &student : branchStudents)
            QVERIFY(student.getCourse().contains("Information Technology", Qt::CaseInsensitive));
        QVERIFY(!store.findStudent("rahul.sharma@placebridge.edu").has_value());
        QVERIFY(!store.addStudent(Student("Blocked", "blocked@example.edu",
            "B.Tech Information Technology", "8.0", "Registered", "BLOCKED1", "", "")));
        store.setActor("rahul.sharma@placebridge.edu");
        QVERIFY(!store.findStudent(added.getEmail()).has_value());
        QVERIFY(store.students().size() == 1);
        QVERIFY(!store.removeStudent(added.getEmail()));
        store.setActor("officer@placebridge.edu");
        const Event conflictingInterview{ QDate(2026, 11, 15), "10:00", "Google panel",
            "Interview", "Panel", "Google", "Scheduled" };
        QVERIFY(!store.interviewConflictReason(conflictingInterview).isEmpty());
        QVERIFY(!store.addEvent(conflictingInterview));
        const Application application{ added.getName(), "Example Corp", "Engineer", "Applied" };
        QVERIFY(store.addApplication(application));
        QVERIFY(store.updateApplicationStatus(added.getName(), application.company,
                                              application.role, "Shortlisted"));
        const QVector<Application> applications = store.applications();
        bool found = false;
        for (const Application &item : applications) {
            if (item.student == added.getName()) {
                QCOMPARE(item.stage(), 1);
                QCOMPARE(item.status, QString("Shortlisted"));
                found = true;
            }
        }
        QVERIFY(found);
        QCOMPARE(store.registerAccount(added.getEmail(), "student-pass", "Student",
                                       added.getName(), added.getCourse()), QString());
        store.setActor(added.getEmail());
        QVERIFY(store.withdrawApplication(added.getName(), application.company, application.role));
        QVERIFY(!store.hasApplied(added.getName(), application.company, application.role));

        store.setActor("officer@placebridge.edu");
        Company notifyDrive("Notify Corp", "", "Analyst", "10 LPA", "Open",
                            QDate::currentDate().addDays(10).toString("yyyy-MM-dd"), "6.0",
                            "Information Technology");
        notifyDrive.setJobDescription("Data platform analyst");
        notifyDrive.setEligibility("No active backlogs");
        notifyDrive.setRounds("Technical, HR");
        QVERIFY(store.addDrive(notifyDrive));
        store.setActor(added.getEmail());
        bool driveDetailsPersisted = false;
        for (const Company &drive : store.drives()) {
            if (drive.getName() != "Notify Corp") continue;
            QCOMPARE(drive.getJobDescription(), QString("Data platform analyst"));
            QCOMPARE(drive.getEligibility(), QString("No active backlogs"));
            QCOMPARE(drive.getRounds(), QString("Technical, HR"));
            QVERIFY(drive.isApproved());
            driveDetailsPersisted = true;
        }
        QVERIFY(driveDetailsPersisted);
        store.setActor("recruiter@accenture.com");
        QVERIFY(store.addDrive(Company("Accenture", "", "Data Engineer", "11 LPA", "Open",
            QDate::currentDate().addDays(20).toString("yyyy-MM-dd"), "7.0")));
        bool pendingApproval = false;
        for (const Company &drive : store.drives())
            if (drive.getJobRole() == "Data Engineer") pendingApproval = !drive.isApproved();
        QVERIFY(pendingApproval);
        store.setActor("officer@placebridge.edu");
        QVERIFY(store.approveDrive("Accenture", "Data Engineer"));
        store.setActor(added.getEmail());
        const Application offer{ added.getName(), "Notify Corp", "Analyst", "Applied" };
        QVERIFY(store.addApplication(offer));
        store.setActor("officer@placebridge.edu");
        QVERIFY(store.updateApplicationStatus(added.getName(), "Notify Corp", "Analyst", "Offer"));
        store.setActor(added.getEmail());
        QVERIFY(store.acceptOffer(added.getEmail(), "Notify Corp", "Analyst"));
        const auto placedStudent = store.findStudent(added.getEmail());
        QVERIFY(placedStudent.has_value());
        QCOMPARE(placedStudent->getStatus(), QString("Placed"));
        QVERIFY(placedStudent->isPlaced());
        bool acceptedRecorded = false;
        for (const Application &application : store.applications())
            if (application.company == "Notify Corp")
                acceptedRecorded = application.offerDecision == "Accepted" && application.stage() == 3;
        QVERIFY(acceptedRecorded);
        store.setActor("officer@placebridge.edu");
        const Event studentInterview{ QDate::currentDate().addDays(3), "09:30",
            "Notify interview", "Interview", "Panel interview", "Notify Corp", "Scheduled" };
        QVERIFY(store.addEvent(studentInterview));
        store.setActor(added.getEmail());
        QVERIFY(store.bookInterviewSlot(added.getEmail(), "Notify Corp", "Analyst", "Notify interview"));
        const Event conflictingStudentInterview{ studentInterview.date, "10:30",
            "Another company interview", "Interview", "Panel interview", "Other Corp", "Scheduled" };
        store.setActor("officer@placebridge.edu");
        QVERIFY(store.addEvent(conflictingStudentInterview));
        store.setActor(added.getEmail());
        QVERIFY(!store.bookInterviewSlot(added.getEmail(), "Other Corp", "Analyst",
                                         "Another company interview"));
        QVERIFY(store.submitDriveFeedback(added.getEmail(), "Notify Corp", "Analyst", 5, "Clear process"));
        QVERIFY(!store.submitDriveFeedback(added.getEmail(), "Notify Corp", "Analyst", 4, "Duplicate"));
        store.setActor("recruiter@accenture.com");
        QVERIFY(!store.findStudent("test.user@example.edu").has_value());
        QVERIFY(!store.events().isEmpty());
        for (const Event &event : store.events())
            QVERIFY(event.company == "Accenture");
        store.setActor(added.getEmail());
        QCOMPARE(store.unreadNotificationCount(added.getEmail()), 5);
        QVERIFY(store.markNotificationsRead(added.getEmail()));
        QCOMPARE(store.unreadNotificationCount(added.getEmail()), 0);
        store.setActor("officer@placebridge.edu");
        QVERIFY(!store.activityLog().isEmpty());
    }
};

QTEST_GUILESS_MAIN(PlaceBridgeTests)
#include "test_placebridge.moc"
