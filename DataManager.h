#ifndef PLACEBRIDGE_DATA_MANAGER_TYPES_H
#define PLACEBRIDGE_DATA_MANAGER_TYPES_H

#include "SecurityManager.h"
#include "SystemServices.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QString>
#include <QStringList>
#include <QTextStream>
#include <utility>

//abstraction: The User class is an abstract base class that defines the common interface and behavior for different types of users in the system. It provides pure virtual functions that must be implemented by derived classes, allowing for polymorphic behavior when working with different user types.
class User {
protected:
    QString name;
    QString email;

public:
    explicit User(QString n, QString e = "");
    virtual ~User();

    virtual QString role() const = 0;                                                              //runtime polymorphism: pure virtual function to be implemented by derived classes to return the role of the user
    virtual Permissions::Role accountRole() const = 0;

    bool canManageStudents() const;
    bool canManageDrives() const;
    bool canManageApplications() const;
    bool canViewActivity() const;
    bool canReadAllStudents() const;
    bool canViewReports() const;

    virtual QString welcome() const;
    QString getName() const;
    void setName(const QString &n);
    QString getEmail() const;
    void setEmail(const QString &e);
};

class FileRepository {
private:
    QString filePath;
    mutable QStringList cache;
    mutable bool loaded = false;

    static QString locateDataDir();

public:
    explicit FileRepository(const QString &filename);

    static QString dataDirectory();
    static QString clean(QString s);

    QString getFilePath() const;
    QStringList rows() const;
    bool append(const QString &line);
    bool overwrite(const QStringList &lines);
    int count() const;
};

#endif // PLACEBRIDGE_DATA_MANAGER_TYPES_H

#if !defined(PLACEBRIDGE_DATA_MANAGER_TYPES_ONLY) && !defined(PLACEBRIDGE_DATA_MANAGER_STORE_H)
#define PLACEBRIDGE_DATA_MANAGER_STORE_H

#include "student/student.h"
#include "company/company.h"

#include <QObject>
#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSettings>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QSaveFile>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QVariant>
#include <QVector>
#include <QUuid>
#include <QHash>
#include <QSet>
#include <algorithm>
#include <optional>
#include <stdexcept>

struct Application {
    QString student, company, role, status;
    QString scorecard = {};
    QString offerDecision = {};

    // Stable identity of the drive this application belongs to, used as a hash key
    // so pages can join applications to drives without a query per row.
    static QString driveKey(const QString &company, const QString &role) {
        return company + QLatin1Char('\x1f') + role;
    }
    QString driveKey() const { return driveKey(company, role); }

    int stage() const {
        const QString value = status.trimmed().toLower();
        if (value == "rejected" || value.startsWith("closed")) return -1;
        if (value.contains("offer")) return 3;
        if (value.contains("interview") || value.contains("technical")) return 2;
        if (value.contains("shortlist") || value.contains("test")) return 1;
        return 0;
    }

    QString toRecord() const {
        return QStringList{ student, company, role, status }.join(QLatin1Char('|'));
    }
    static Application fromRecord(const QString &line) {
        const QStringList p = line.split('|');
        return { p.value(0), p.value(1), p.value(2), p.value(3) };
    }
};

struct Event {
    QDate date;
    QString time, title, type, detail, company, state;
};

struct Account {
    QString email, salt, hash, role, name, extra;
};

struct ActivityEntry {
    qint64 id = 0;
    QString actor, action, details;
    QDateTime occurredAt;
};

struct UserNotification {
    qint64 id = 0;
    QString title, body, type;
    QDateTime createdAt;
    bool read = false;
};

struct VaultDocument {
    qint64 id = 0;
    QString name;
    QByteArray content;
    bool verified = false;
};

class DataStore : public QObject {
    Q_OBJECT

    QSqlDatabase db;
    QString connectionName;
    QString actorEmail;
    mutable QString lastErrorText;
    mutable int remainingLockout = 0;

    // The signed-in user's role is read on nearly every store call (permission
    // checks) but changes only when the actor or the accounts table does, so it
    // is cached instead of costing one SELECT per check.
    mutable QString cachedActorRole;
    mutable Permissions::Role cachedActorRoleId = Permissions::Role::None;
    mutable bool actorRoleCached = false;

    void invalidateActorRole() const {
        actorRoleCached = false;
        cachedActorRole.clear();
        cachedActorRoleId = Permissions::Role::None;
    }

    QString actorRole() const {
        if (actorRoleCached) return cachedActorRole;
        actorRoleCached = true;
        if (actorEmail.isEmpty()) return cachedActorRole;
        QSqlQuery q(db);
        q.prepare("SELECT role FROM accounts WHERE email=?");
        q.addBindValue(actorEmail);
        if (q.exec() && q.next()) cachedActorRole = q.value(0).toString();
        cachedActorRoleId = Permissions::fromName(cachedActorRole);
        return cachedActorRole;
    }

    // Enum form of actorRole(); avoids re-parsing the role name on hot paths.
    Permissions::Role actorRoleId() const {
        actorRole();
        return cachedActorRoleId;
    }

    bool isAdminActor() const { return Permissions::isAdmin(actorRoleId()); }

    static bool validEmailAddress(const QString &email) {
        static const QRegularExpression pattern(
            "^[A-Z0-9.!#$%&'*+/=?^_`{|}~-]+@[A-Z0-9](?:[A-Z0-9-]{0,61}[A-Z0-9])?"
            "(?:\\.[A-Z0-9](?:[A-Z0-9-]{0,61}[A-Z0-9])?)+$",
            QRegularExpression::CaseInsensitiveOption);
        return pattern.match(email.trimmed()).hasMatch();
    }

    bool canReadStudent(const QString &email) const {
        const Permissions::Role role = actorRoleId();
        if (Permissions::isAdmin(role)) return true;
        if (Permissions::isStudent(role))
            return actorEmail.compare(email, Qt::CaseInsensitive) == 0;
        if (Permissions::isCoordinator(role)) {
            const auto account = actorAccount();
            const auto student = findStudentForPermission(email);
            return account && student &&
                student->getCourse().contains(account->extra, Qt::CaseInsensitive);
        }
        if (!Permissions::isRecruiter(role)) return false;
        const auto account = actorAccount();
        if (!account) return false;
        QSqlQuery q(db);
        q.prepare("SELECT 1 FROM applications a JOIN drives d ON d.company=a.company AND d.role=a.role "
                  "JOIN students s ON s.name=a.student WHERE s.email=? AND d.company=? LIMIT 1");
        q.addBindValue(email);
        q.addBindValue(account->extra);
        return q.exec() && q.next();
    }

    std::optional<Student> findStudentForPermission(const QString &email) const {
        QSqlQuery q(db);
        q.prepare("SELECT name,email,course,cgpa,status,roll_no,skills,test_score FROM students WHERE email=?");
        q.addBindValue(email);
        if (!q.exec() || !q.next()) return std::nullopt;
        return Student(q.value(0).toString(), q.value(1).toString(), q.value(2).toString(),
                       q.value(3).toString(), q.value(4).toString(), q.value(5).toString(),
                       q.value(6).toString(), q.value(7).toString());
    }

    QString studentNameForEmail(const QString &email) const {
        QSqlQuery q(db);
        q.prepare("SELECT name FROM students WHERE email=?");
        q.addBindValue(email);
        return q.exec() && q.next() ? q.value(0).toString() : QString();
    }

    bool recruiterOwnsApplication(const QString &student, const QString &company,
                                  const QString &role) const {
        const auto account = actorAccount();
        if (!account) return false;
        QSqlQuery q(db);
        q.prepare("SELECT 1 FROM applications a JOIN drives d ON d.company=a.company AND d.role=a.role "
                  "WHERE a.student=? AND a.company=? AND a.role=? AND d.company=?");
        q.addBindValue(student);
        q.addBindValue(company);
        q.addBindValue(role);
        q.addBindValue(account->extra);
        return q.exec() && q.next();
    }

    bool recruiterHasStudent(const QString &student) const {
        const auto account = actorAccount();
        if (!account) return false;
        QSqlQuery q(db);
        q.prepare("SELECT 1 FROM applications a JOIN drives d ON d.company=a.company AND d.role=a.role "
                  "WHERE a.student=? AND d.company=? LIMIT 1");
        q.addBindValue(student);
        q.addBindValue(account->extra);
        return q.exec() && q.next();
    }

    // The actor's own account row (its `extra` column carries the recruiter's
    // company and the coordinator's branch) is needed by most permission checks.
    mutable std::optional<Account> cachedActorAccount;
    mutable bool actorAccountCached = false;

    std::optional<Account> actorAccount() const {
        if (!actorAccountCached) {
            actorAccountCached = true;
            cachedActorAccount = actorEmail.isEmpty() ? std::nullopt : accountByEmail(actorEmail);
        }
        return cachedActorAccount;
    }

    void invalidateActorCache() const {
        invalidateActorRole();
        actorAccountCached = false;
        cachedActorAccount.reset();
    }

    std::optional<Account> accountByEmail(const QString &email) const {
        QSqlQuery q(db);
        q.prepare("SELECT email,salt,password_hash,role,name,extra FROM accounts WHERE email=?");
        q.addBindValue(email.trimmed());
        if (!q.exec()) {
            reportQueryError(q, "Finding account");
            return std::nullopt;
        }
        if (!q.next()) return std::nullopt;
        return Account{ q.value(0).toString(), q.value(1).toString(), q.value(2).toString(),
                        q.value(3).toString(), q.value(4).toString(), q.value(5).toString() };
    }

    // Hashing lives in PasswordHasher; these thin wrappers keep the call sites short.
    static QByteArray randomSalt() { return PasswordHasher::randomSalt(); }
    static QByteArray pbkdf2(const QByteArray &password, const QByteArray &salt) {
        return PasswordHasher::pbkdf2(password, salt);
    }
    static bool constantTimeEqual(const QByteArray &a, const QByteArray &b) {
        return PasswordHasher::constantTimeEqual(a, b);
    }

    static QString defaultDatabasePath() {
        const QString configured = QSettings().value("data/folder").toString();
        const QString sharedDataFolder = qEnvironmentVariable("PLACEBRIDGE_DATA_DIR");
        QString dir = !sharedDataFolder.isEmpty() ? sharedDataFolder : configured.isEmpty()
            ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) : configured;
        if (dir.isEmpty()) dir = QDir::currentPath();
        if (!QDir().mkpath(dir))
            throw std::runtime_error("Could not create the PlaceBridge application data directory.");
        return dir + "/placebridge.sqlite";
    }

    bool reportQueryError(const QSqlQuery &query, const QString &operation) const {
        lastErrorText = operation + ": " + query.lastError().text();
        AppLogger::write("ERROR", lastErrorText);
        qWarning().noquote() << lastErrorText;
        return false;
    }

    bool execSql(const QString &sql, const QString &operation) {
        QSqlQuery query(db);
        if (!query.exec(sql)) return reportQueryError(query, operation);
        return true;
    }

    bool migrateLegacyData() {
        const QStringList tables = { "students", "drives", "applications", "events", "accounts" };
        for (const QString &table : tables) {
            QSqlQuery count(db);
            if (!count.exec("SELECT COUNT(*) FROM " + table) || !count.next())
                return reportQueryError(count, "Checking legacy migration target");
            if (count.value(0).toInt() != 0) continue;

            const QString filename = table == "drives" ? "companies.txt" : table + ".txt";
            FileRepository repository(filename);
            const QStringList rows = repository.rows();
            if (rows.isEmpty()) continue;
            if (!db.transaction()) {
                lastErrorText = "Could not start legacy data migration: " + db.lastError().text();
                return false;
            }
            bool ok = true;
            for (const QString &row : rows) {
                const QStringList fields = row.split('|');
                if (table == "students") {
                    ok = insertStudent(Student::fromRecord(row)) && ok;
                } else if (table == "drives") {
                    ok = insertDrive(Company::fromRecord(row)) && ok;
                } else if (table == "applications") {
                    ok = insertApplication(Application::fromRecord(row)) && ok;
                } else if (table == "accounts" && fields.size() >= 6) {
                    QSqlQuery q(db);
                    q.prepare("INSERT INTO accounts(email,salt,password_hash,role,name,extra) VALUES(?,?,?,?,?,?)");
                    q.addBindValue(fields[0].trimmed().toLower());
                    q.addBindValue(fields[1]);
                    q.addBindValue("legacy-sha256:" + fields[2]);
                    q.addBindValue(fields[3]);
                    q.addBindValue(fields[4]);
                    q.addBindValue(fields[5]);
                    ok = (q.exec() || reportQueryError(q, "Migrating legacy account")) && ok;
                } else if (table == "events" && fields.size() >= 7) {
                    QSqlQuery q(db);
                    q.prepare("INSERT INTO events(event_date,event_time,title,type,detail,company,state) "
                              "VALUES(?,?,?,?,?,?,?)");
                    for (int i = 0; i < 7; ++i) q.addBindValue(fields[i]);
                    ok = (q.exec() || reportQueryError(q, "Migrating legacy event")) && ok;
                }
            }
            if (!ok || !db.commit()) {
                db.rollback();
                lastErrorText = "Legacy text-data migration failed for " + table + ".";
                AppLogger::write("ERROR", lastErrorText);
                return false;
            }
            logActivity("migrate legacy data", filename);
        }
        return true;
    }

    bool createSchema() {
        const QStringList statements = {
            "CREATE TABLE IF NOT EXISTS students ("
            "name TEXT NOT NULL, email TEXT NOT NULL COLLATE NOCASE PRIMARY KEY, course TEXT NOT NULL,"
            "cgpa TEXT NOT NULL DEFAULT 'N/A', status TEXT NOT NULL DEFAULT 'Registered',"
            "roll_no TEXT NOT NULL DEFAULT '', skills TEXT NOT NULL DEFAULT '', test_score TEXT NOT NULL DEFAULT '',"
            "backlogs INTEGER NOT NULL DEFAULT 0, batch_year INTEGER NOT NULL DEFAULT 0,"
            "verified INTEGER NOT NULL DEFAULT 0, projects TEXT NOT NULL DEFAULT '',"
            "achievements TEXT NOT NULL DEFAULT '', links TEXT NOT NULL DEFAULT '')",
            "CREATE UNIQUE INDEX IF NOT EXISTS idx_students_roll ON students(roll_no COLLATE NOCASE) WHERE roll_no <> ''",
            "CREATE TABLE IF NOT EXISTS drives ("
            "company TEXT NOT NULL, role TEXT NOT NULL, package TEXT NOT NULL, status TEXT NOT NULL,"
            "drive_date TEXT NOT NULL, min_cgpa TEXT NOT NULL DEFAULT '', allowed_branches TEXT NOT NULL DEFAULT '',"
            "approved INTEGER NOT NULL DEFAULT 1, job_description TEXT NOT NULL DEFAULT '',"
            "eligibility TEXT NOT NULL DEFAULT '', rounds TEXT NOT NULL DEFAULT '',"
            "PRIMARY KEY(company, role))",
            "CREATE TABLE IF NOT EXISTS applications ("
            "student TEXT NOT NULL, company TEXT NOT NULL, role TEXT NOT NULL, status TEXT NOT NULL,"
            "offer_decision TEXT NOT NULL DEFAULT '', scorecard TEXT NOT NULL DEFAULT '',"
            "PRIMARY KEY(student, company, role))",
            "CREATE TABLE IF NOT EXISTS events ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, event_date TEXT NOT NULL, event_time TEXT NOT NULL,"
            "title TEXT NOT NULL, type TEXT NOT NULL, detail TEXT NOT NULL, company TEXT NOT NULL, state TEXT NOT NULL)",
            "CREATE TABLE IF NOT EXISTS accounts ("
            "email TEXT NOT NULL COLLATE NOCASE PRIMARY KEY, salt TEXT NOT NULL, password_hash TEXT NOT NULL,"
            "role TEXT NOT NULL, name TEXT NOT NULL, extra TEXT NOT NULL DEFAULT '',"
            "failed_attempts INTEGER NOT NULL DEFAULT 0, locked_until INTEGER NOT NULL DEFAULT 0)",
            "CREATE INDEX IF NOT EXISTS idx_accounts_email ON accounts(email COLLATE NOCASE)",
            "CREATE INDEX IF NOT EXISTS idx_students_email ON students(email COLLATE NOCASE)",
            "CREATE TABLE IF NOT EXISTS activity_log ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, actor TEXT NOT NULL, action TEXT NOT NULL,"
            "details TEXT NOT NULL, occurred_at TEXT NOT NULL)",
            "CREATE INDEX IF NOT EXISTS idx_activity_time ON activity_log(occurred_at DESC)",
            "CREATE TABLE IF NOT EXISTS notifications ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, recipient_email TEXT NOT NULL COLLATE NOCASE,"
            "title TEXT NOT NULL, body TEXT NOT NULL, type TEXT NOT NULL, created_at TEXT NOT NULL,"
            "is_read INTEGER NOT NULL DEFAULT 0)",
            "CREATE TABLE IF NOT EXISTS documents ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, student_email TEXT NOT NULL COLLATE NOCASE,"
            "display_name TEXT NOT NULL, content BLOB NOT NULL, verified INTEGER NOT NULL DEFAULT 0,"
            "uploaded_at TEXT NOT NULL)",
            "CREATE TABLE IF NOT EXISTS interview_slots ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, student_email TEXT NOT NULL COLLATE NOCASE,"
            "company TEXT NOT NULL, role TEXT NOT NULL, event_id INTEGER NOT NULL DEFAULT 0,"
            "booked_at TEXT NOT NULL, UNIQUE(student_email,company,role,event_id))",
            "CREATE TABLE IF NOT EXISTS feedback ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, student_email TEXT NOT NULL COLLATE NOCASE,"
            "company TEXT NOT NULL, role TEXT NOT NULL, rating INTEGER NOT NULL, comments TEXT NOT NULL,"
            "created_at TEXT NOT NULL)",
            "CREATE TABLE IF NOT EXISTS announcements ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, branch TEXT NOT NULL DEFAULT '',"
            "message TEXT NOT NULL, created_at TEXT NOT NULL, author TEXT NOT NULL)",
            "CREATE TABLE IF NOT EXISTS company_contacts ("
            "company TEXT PRIMARY KEY, contact_name TEXT NOT NULL DEFAULT '',"
            "contact_email TEXT NOT NULL DEFAULT '', mou_status TEXT NOT NULL DEFAULT '', notes TEXT NOT NULL DEFAULT '')",
            "CREATE INDEX IF NOT EXISTS idx_notifications_recipient ON notifications(recipient_email, is_read)",
            "CREATE INDEX IF NOT EXISTS idx_applications_student ON applications(student)",
            "CREATE INDEX IF NOT EXISTS idx_applications_company ON applications(company, role)",
            "CREATE INDEX IF NOT EXISTS idx_events_date ON events(event_date, event_time)",
            "CREATE INDEX IF NOT EXISTS idx_events_company ON events(company)",
            "CREATE INDEX IF NOT EXISTS idx_documents_student ON documents(student_email)",
            "CREATE INDEX IF NOT EXISTS idx_students_name ON students(name)"
        };
        for (const QString &statement : statements)
            if (!execSql(statement, "Creating SQLite schema")) return false;

        // Add the optional drive branch filter to databases created by earlier PlaceBridge builds.
        QSqlQuery columns(db);
        if (!columns.exec("PRAGMA table_info(drives)"))
            return reportQueryError(columns, "Inspecting drives schema");
        bool hasAllowedBranches = false;
        while (columns.next())
            hasAllowedBranches |= columns.value("name").toString() == "allowed_branches";
        if (!hasAllowedBranches && !execSql(
                "ALTER TABLE drives ADD COLUMN allowed_branches TEXT NOT NULL DEFAULT ''",
                "Adding allowed branches to drives"))
            return false;
        QSqlQuery accountColumns(db);
        if (!accountColumns.exec("PRAGMA table_info(accounts)"))
            return reportQueryError(accountColumns, "Inspecting accounts schema");
        bool hasFailedAttempts = false;
        bool hasLockedUntil = false;
        while (accountColumns.next()) {
            hasFailedAttempts |= accountColumns.value("name").toString() == "failed_attempts";
            hasLockedUntil |= accountColumns.value("name").toString() == "locked_until";
        }
        if (!hasFailedAttempts && !execSql(
                "ALTER TABLE accounts ADD COLUMN failed_attempts INTEGER NOT NULL DEFAULT 0",
                "Adding login failure tracking")) return false;
        if (!hasLockedUntil && !execSql(
                "ALTER TABLE accounts ADD COLUMN locked_until INTEGER NOT NULL DEFAULT 0",
                "Adding login lockout tracking")) return false;
        if (!ensureColumn("students", "backlogs", "INTEGER NOT NULL DEFAULT 0") ||
            !ensureColumn("students", "batch_year", "INTEGER NOT NULL DEFAULT 0") ||
            !ensureColumn("students", "verified", "INTEGER NOT NULL DEFAULT 0") ||
            !ensureColumn("students", "projects", "TEXT NOT NULL DEFAULT ''") ||
            !ensureColumn("students", "achievements", "TEXT NOT NULL DEFAULT ''") ||
            !ensureColumn("students", "links", "TEXT NOT NULL DEFAULT ''") ||
            !ensureColumn("drives", "approved", "INTEGER NOT NULL DEFAULT 1") ||
            !ensureColumn("drives", "job_description", "TEXT NOT NULL DEFAULT ''") ||
            !ensureColumn("drives", "eligibility", "TEXT NOT NULL DEFAULT ''") ||
            !ensureColumn("drives", "rounds", "TEXT NOT NULL DEFAULT ''") ||
            !ensureColumn("applications", "offer_decision", "TEXT NOT NULL DEFAULT ''") ||
            !ensureColumn("applications", "scorecard", "TEXT NOT NULL DEFAULT ''") ||
            !ensureColumn("accounts", "reset_salt", "TEXT NOT NULL DEFAULT ''") ||
            !ensureColumn("accounts", "reset_hash", "TEXT NOT NULL DEFAULT ''") ||
            !ensureColumn("accounts", "reset_expires", "INTEGER NOT NULL DEFAULT 0"))
            return false;
        return true;
    }

    bool ensureColumn(const QString &table, const QString &column, const QString &definition) {
        QSqlQuery query(db);
        if (!query.exec("PRAGMA table_info(" + table + ")"))
            return reportQueryError(query, "Inspecting " + table + " schema");
        while (query.next())
            if (query.value("name").toString() == column) return true;
        return execSql(QString("ALTER TABLE %1 ADD COLUMN %2 %3")
                           .arg(table, column, definition),
                       "Migrating " + table + "." + column);
    }

    static bool validBackupDatabase(const QString &path) {
        const QString name = "PlaceBridge_backup_validation_" +
            QUuid::createUuid().toString(QUuid::Id128);
        bool valid = false;
        {
            QSqlDatabase check = QSqlDatabase::addDatabase("QSQLITE", name);
            check.setDatabaseName(path);
            if (check.open()) {
                QSqlQuery query(check);
                query.prepare("SELECT COUNT(*) FROM sqlite_master WHERE type='table' "
                              "AND name IN ('students','drives','applications','accounts')");
                valid = query.exec() && query.next() && query.value(0).toInt() == 4;
                check.close();
            }
        }
        QSqlDatabase::removeDatabase(name);
        return valid;
    }

    bool tableIsEmpty(const QString &table, bool &empty) const {
        QSqlQuery query(db);
        if (!query.exec("SELECT 1 FROM " + table + " LIMIT 1"))
            return reportQueryError(query, "Checking " + table + " table");
        empty = !query.next();
        return true;
    }

    bool insertStudent(const Student &student) {
        QSqlQuery q(db);
        q.prepare("INSERT INTO students(name,email,course,cgpa,status,roll_no,skills,test_score) "
                  "VALUES(?,?,?,?,?,?,?,?)");
        q.addBindValue(student.getName());
        q.addBindValue(student.getEmail().trimmed().toLower());
        q.addBindValue(student.getCourse());
        q.addBindValue(student.getCgpa());
        q.addBindValue(student.getStatus());
        q.addBindValue(student.getRollNo());
        q.addBindValue(student.getSkills());
        q.addBindValue(student.getTestScore());
        return q.exec() || reportQueryError(q, "Inserting student");
    }

    bool insertDrive(const Company &drive) {
        QSqlQuery q(db);
        q.prepare("INSERT INTO drives(company,role,package,status,drive_date,min_cgpa,allowed_branches,"
                  "job_description,eligibility,rounds,approved) VALUES(?,?,?,?,?,?,?,?,?,?,?)");
        q.addBindValue(drive.getName());
        q.addBindValue(drive.getJobRole());
        q.addBindValue(drive.getPackageOffer());
        q.addBindValue(drive.getDriveStatus());
        q.addBindValue(drive.getDriveDate());
        q.addBindValue(drive.getMinCgpa());
        q.addBindValue(drive.getAllowedBranches());
        q.addBindValue(drive.getJobDescription());
        q.addBindValue(drive.getEligibility());
        q.addBindValue(drive.getRounds());
        const QString role = actorRole();
        q.addBindValue(role.isEmpty() ? (drive.isApproved() ? 1 : 0)
                                      : (Permissions::isAdmin(role) ? 1 : 0));
        return q.exec() || reportQueryError(q, "Inserting drive");
    }

    bool insertApplication(const Application &application) {
        QSqlQuery q(db);
        q.prepare("INSERT INTO applications(student,company,role,status) VALUES(?,?,?,?)");
        q.addBindValue(application.student);
        q.addBindValue(application.company);
        q.addBindValue(application.role);
        q.addBindValue(application.status);
        return q.exec() || reportQueryError(q, "Inserting application");
    }

    bool seed() {
        if (!db.transaction()) {
            lastErrorText = db.lastError().text();
            qWarning().noquote() << "Starting demo-data transaction:" << lastErrorText;
            return false;
        }
        bool ok = true;
        bool empty = false;
        if (!tableIsEmpty("students", empty)) ok = false;
        if (ok && empty) {
            const QVector<Student> records = {
                { "Rahul Sharma", "rahul.sharma@placebridge.edu", "B.Tech Computer Science", "8.9", "Shortlisted", "21CS014", "Java, C++, SQL", "86" },
                { "Sneha Patel", "sneha.patel@placebridge.edu", "B.Tech Information Technology", "9.2", "Selected", "21IT009", "Distributed Systems, Cloud", "94" },
                { "Amit Verma", "amit.verma@placebridge.edu", "B.Tech Electronics & Comm", "7.8", "Registered", "21EC031", "Embedded C, Verilog", "74" },
                { "Pooja Nair", "pooja.nair@placebridge.edu", "B.Tech Computer Science", "8.6", "Selected", "21CS022", "SQL, Python, Docker", "82" },
                { "Vikram Singh", "vikram.singh@placebridge.edu", "B.Tech Mechanical Eng", "6.9", "Registered", "21ME017", "CAD, Thermodynamics", "63" },
                { "Neha Joshi", "neha.joshi@placebridge.edu", "B.Tech Computer Science", "9.1", "Shortlisted", "21CS027", "Python, React, Cloud", "91" },
                { "Rohan Desai", "rohan.desai@placebridge.edu", "B.Tech Electrical Eng", "7.4", "Registered", "21EE012", "C++, Embedded, IoT", "69" }
            };
            for (const Student &s : records) ok = insertStudent(s) && ok;
        }
        if (!tableIsEmpty("drives", empty)) ok = false;
        if (ok && empty) {
            const QVector<Company> records = {
                { "Google", "", "Software Engineer", "28 LPA", "Open", "2026-11-18", "8.5" },
                { "Microsoft", "", "Cloud Solutions Architect", "24 LPA", "Open", "2026-11-15", "8.0" },
                { "Amazon", "", "Systems Development Engineer", "22 LPA", "Open", "2026-11-22", "7.5" },
                { "Infosys", "", "Systems Engineer Specialist", "9.5 LPA", "Open", "2026-10-20", "7.5" },
                { "Accenture", "", "Technology Consultant", "8.0 LPA", "Scheduled", "2026-11-03", "6.5" },
                { "TCS Digital", "", "Associate Software Engineer", "7.5 LPA", "Completed", "2026-10-24", "7.0" },
                { "L&T Technology", "", "Core Systems Engineer", "7.2 LPA", "Open", "2026-11-11", "6.5" }
            };
            for (const Company &d : records) ok = insertDrive(d) && ok;
        }
        if (!tableIsEmpty("applications", empty)) ok = false;
        if (ok && empty) {
            const QVector<Application> records = {
                { "Rahul Sharma", "Infosys", "Systems Engineer Specialist", "Offer Extended" },
                { "Rahul Sharma", "TCS Digital", "Associate Software Engineer", "Interview Round" },
                { "Rahul Sharma", "Accenture", "Technology Consultant", "Test Scheduled" },
                { "Sneha Patel", "Google", "Software Engineer", "Offer Extended" },
                { "Sneha Patel", "Microsoft", "Cloud Solutions Architect", "Interview" },
                { "Pooja Nair", "Amazon", "Systems Development Engineer", "Offer Extended" },
                { "Pooja Nair", "TCS Digital", "Associate Software Engineer", "Shortlisted" },
                { "Amit Verma", "L&T Technology", "Core Systems Engineer", "Under Review" },
                { "Vikram Singh", "Infosys", "Systems Engineer Specialist", "Applied" },
                { "Neha Joshi", "Google", "Software Engineer", "Technical Round 2" },
                { "Rohan Desai", "L&T Technology", "Core Systems Engineer", "Under Review" }
            };
            for (const Application &a : records) ok = insertApplication(a) && ok;
        }
        if (!tableIsEmpty("events", empty)) ok = false;
        if (ok && empty) {
            const QStringList records = {
                "2026-10-02|10:00|Cognizant Drive|Completed|Campus presentation and aptitude round|Cognizant|Completed",
                "2026-10-09|11:00|Capgemini Test|Assessment|Online aptitude assessment|Capgemini|Completed",
                "2026-10-11|10:00|L&T Interviews|Interview|Panel interviews|L&T Technology|Completed",
                "2026-10-20|10:00|Infosys Online|Assessment|Online Coding Assessment (Lab 3)|Infosys|Open",
                "2026-10-24|10:00|TCS Digital Drive|Interview|Technical & HR Interview Panels|TCS Digital|Scheduled",
                "2026-10-29|10:00|Wipro Technical|Interview|Technical Evaluation Stage|Wipro|Scheduled",
                "2026-11-03|14:00|Accenture Test|Assessment|Online technical assessment|Accenture|Scheduled",
                "2026-11-03|10:00|Interview: Sneha Patel|Interview|Technical Evaluation Panel 1|Accenture|Confirmed",
                "2026-11-03|11:00|Interview: Pooja Nair|Interview|Technical Evaluation Panel 1|Accenture|Confirmed",
                "2026-11-03|12:00|Interview: Rahul Sharma|Interview|HR & Leadership Discussion|Accenture|Awaiting Reply",
                "2026-11-11|10:00|L&T Drive|Assessment|Core systems screening|L&T Technology|Scheduled",
                "2026-11-15|10:00|Microsoft Drive|Interview|Technical Screening + Panel|Microsoft|Scheduled",
                "2026-11-18|10:00|Google Drive|Assessment|Online Coding + System Design|Google|Open"
            };
            for (const QString &line : records) {
                const QStringList p = line.split('|');
                QSqlQuery q(db);
                q.prepare("INSERT INTO events(event_date,event_time,title,type,detail,company,state) "
                          "VALUES(?,?,?,?,?,?,?)");
                for (int i = 0; i < 7; ++i) q.addBindValue(p.value(i));
                ok = (q.exec() || reportQueryError(q, "Seeding event")) && ok;
            }
        }
        if (!tableIsEmpty("accounts", empty)) ok = false;
        if (ok && empty) {
            const struct SeedAccount { const char *email; const char *password; const char *role; const char *name; const char *extra; } accounts[] = {
                { "officer@placebridge.edu", "admin123", "Admin", "Prof. Rajesh Verma", "PO-2024" },
                { "rahul.sharma@placebridge.edu", "student123", "Student", "Rahul Sharma", "B.Tech Computer Science" },
                { "recruiter@accenture.com", "recruit123", "Recruiter", "Accenture Campus Hiring", "Accenture" }
            };
            for (const auto &account : accounts) {
                const QString salt = QString::fromLatin1(randomSalt());
                QSqlQuery q(db);
                q.prepare("INSERT INTO accounts(email,salt,password_hash,role,name,extra) VALUES(?,?,?,?,?,?)");
                q.addBindValue(QString::fromLatin1(account.email));
                q.addBindValue(salt);
                q.addBindValue(QString::fromLatin1(pbkdf2(QByteArray(account.password), QByteArray::fromHex(salt.toLatin1()))));
                q.addBindValue(QString::fromLatin1(account.role));
                q.addBindValue(QString::fromLatin1(account.name));
                q.addBindValue(QString::fromLatin1(account.extra));
                ok = (q.exec() || reportQueryError(q, "Seeding account")) && ok;
            }
        }
        if (!ok || !db.commit()) {
            lastErrorText = db.lastError().text();
            db.rollback();
            return false;
        }
        return true;
    }

    // How many offers a student has accepted and the best tier among them;
    // both inputs to PlacementPolicy, read in a single aggregate query.
    struct OfferStanding {
        int acceptedOffers = 0;
        int highestCategory = -1;
    };

    bool offerStanding(const QString &studentName, OfferStanding &standing) const {
        QSqlQuery q(db);
        q.prepare("SELECT COUNT(*),COALESCE(MAX(CASE "
                  "WHEN CAST(REPLACE(LOWER(d.package),' lpa','') AS REAL)>12 THEN 2 "
                  "WHEN CAST(REPLACE(LOWER(d.package),' lpa','') AS REAL)>=8 THEN 1 ELSE 0 END),-1) "
                  "FROM applications a JOIN drives d ON d.company=a.company AND d.role=a.role "
                  "WHERE a.student=? AND a.offer_decision='Accepted'");
        q.addBindValue(studentName);
        if (!q.exec() || !q.next()) return reportQueryError(q, "Checking student offer count");
        standing.acceptedOffers = q.value(0).toInt();
        standing.highestCategory = q.value(1).toInt();
        return true;
    }

    void addNotification(const QString &email, const QString &title, const QString &body, const QString &type) {
        QSqlQuery q(db);
        q.prepare("INSERT INTO notifications(recipient_email,title,body,type,created_at,is_read) VALUES(?,?,?,?,?,0)");
        q.addBindValue(email);
        q.addBindValue(title);
        q.addBindValue(body);
        q.addBindValue(type);
        q.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        if (!q.exec()) reportQueryError(q, "Creating notification");
    }

    void notifyAllStudents(const QString &title, const QString &body, const QString &type) {
        QSqlQuery q(db);
        if (!q.exec("SELECT email FROM students")) {
            reportQueryError(q, "Loading notification recipients");
            return;
        }
        QStringList recipients;
        while (q.next()) recipients.append(q.value(0).toString());
        for (const QString &email : recipients) addNotification(email, title, body, type);
    }

public:
    explicit DataStore(QObject *parent = nullptr)
        : DataStore(defaultDatabasePath(), parent) {}

    explicit DataStore(const QString &databasePath, QObject *parent = nullptr)
        : QObject(parent), connectionName("PlaceBridge_" + QUuid::createUuid().toString(QUuid::Id128)) {
        db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        db.setDatabaseName(databasePath);
        if (!db.open())
            throw std::runtime_error(QString("Could not open PlaceBridge SQLite database: %1")
                                         .arg(db.lastError().text()).toStdString());
        execSql("PRAGMA journal_mode = WAL", "Enabling SQLite write-ahead logging");
        execSql("PRAGMA synchronous = NORMAL", "Tuning SQLite durability");
        execSql("PRAGMA temp_store = MEMORY", "Tuning SQLite temp storage");
        if (!execSql("PRAGMA foreign_keys = ON", "Enabling SQLite foreign keys") ||
            !createSchema() || !migrateLegacyData() || !seed())
            throw std::runtime_error(lastErrorText.toStdString());
    }

    ~DataStore() override {
        db.close();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName);
    }

    QString lastError() const { return lastErrorText; }
    void setActor(const QString &email) {
        actorEmail = email.trimmed();
        invalidateActorCache();
    }
    QString actor() const { return actorEmail; }
    bool adminActor() const { return isAdminActor(); }

    QVector<Student> students() const {
        QVector<Student> out;
        QSqlQuery q(db);
        const Permissions::Role role = actorRoleId();
        QString sql = "SELECT name,email,course,cgpa,status,roll_no,skills,test_score,backlogs,batch_year,"
                      "verified,projects,achievements,links FROM students";
        if (Permissions::isStudent(role)) sql += " WHERE email=?";
        else if (Permissions::isRecruiter(role))
            sql += " WHERE email IN (SELECT s.email FROM students s JOIN applications a ON a.student=s.name "
                   "JOIN drives d ON d.company=a.company AND d.role=a.role WHERE d.company="
                   "(SELECT extra FROM accounts WHERE email=?))";
        else if (Permissions::isCoordinator(role))
            sql += " WHERE course LIKE (SELECT '%'||extra||'%' FROM accounts WHERE email=?)";
        else if (!Permissions::isAdmin(role)) return out;
        sql += " ORDER BY name";
        q.prepare(sql);
        if (sql.contains('?')) q.addBindValue(actorEmail);
        if (!q.exec()) {
            reportQueryError(q, "Loading students");
            return out;
        }
        while (q.next()) {
            Student student(q.value(0).toString(), q.value(1).toString(), q.value(2).toString(),
                            q.value(3).toString(), q.value(4).toString(), q.value(5).toString(),
                            q.value(6).toString(), q.value(7).toString());
            student.setBacklogs(q.value(8).toInt());
            student.setBatchYear(q.value(9).toInt());
            student.setVerified(q.value(10).toInt() != 0);
            student.setProjects(q.value(11).toString());
            student.setAchievements(q.value(12).toString());
            student.setLinks(q.value(13).toString());
            out.append(student);
        }
        return out;
    }

    std::optional<Student> findStudent(const QString &email) const {
        if (!canReadStudent(email)) return std::nullopt;
        QSqlQuery q(db);
        q.prepare("SELECT name,email,course,cgpa,status,roll_no,skills,test_score,backlogs,batch_year,"
                  "verified,projects,achievements,links FROM students WHERE email=?");
        q.addBindValue(email.trimmed());
        if (!q.exec()) {
            reportQueryError(q, "Finding student");
            return std::nullopt;
        }
        if (!q.next()) return std::nullopt;
        Student student(q.value(0).toString(), q.value(1).toString(), q.value(2).toString(),
                        q.value(3).toString(), q.value(4).toString(), q.value(5).toString(),
                        q.value(6).toString(), q.value(7).toString());
        student.setBacklogs(q.value(8).toInt());
        student.setBatchYear(q.value(9).toInt());
        student.setVerified(q.value(10).toInt() != 0);
        student.setProjects(q.value(11).toString());
        student.setAchievements(q.value(12).toString());
        student.setLinks(q.value(13).toString());
        return student;
    }

    bool hasStudentRollNumber(const QString &rollNo, const QString &exceptEmail = {}) const {
        if (rollNo.trimmed().isEmpty()) return false;
        QSqlQuery q(db);
        const bool excludingEmail = !exceptEmail.trimmed().isEmpty();
        q.prepare(QString("SELECT 1 FROM students WHERE lower(roll_no)=lower(?)%1")
                      .arg(excludingEmail ? " AND lower(email)<>lower(?)" : ""));
        q.addBindValue(rollNo.trimmed());
        if (excludingEmail) q.addBindValue(exceptEmail.trimmed());
        if (!q.exec()) {
            reportQueryError(q, "Checking student roll number");
            return false;
        }
        return q.next();
    }

    bool addStudent(const Student &s) {
        if (!isAdminActor() && !(Permissions::isStudent(actorRoleId()) &&
                                  actorEmail.compare(s.getEmail(), Qt::CaseInsensitive) == 0)) return false;
        bool cgpaOk = false;
        const double cgpa = s.getCgpa().toDouble(&cgpaOk);
        const QString cgpaText = s.getCgpa().trimmed();
        const bool cgpaNotProvided = cgpaText.isEmpty() || cgpaText.compare("N/A", Qt::CaseInsensitive) == 0;
        if (s.getName().trimmed().isEmpty() || !validEmailAddress(s.getEmail()) ||
            s.getCourse().trimmed().isEmpty() ||
            (!cgpaNotProvided && (!cgpaOk || cgpa < 0 || cgpa > 10)))
            return false;
        if (findStudent(s.getEmail()) || hasStudentRollNumber(s.getRollNo())) return false;
        if (!insertStudent(s)) return false;
        logActivity("add student", s.getName() + " (" + s.getEmail() + ")");
        emit changed();
        return true;
    }

    int addStudents(const QVector<Student> &list) {
        if (!isAdminActor()) return 0;
        int added = 0;
        if (!db.transaction()) return 0;
        for (const Student &s : list) {
            bool cgpaOk = false;
            const double cgpa = s.getCgpa().toDouble(&cgpaOk);
            const QString cgpaText = s.getCgpa().trimmed();
            const bool cgpaNotProvided = cgpaText.isEmpty() ||
                cgpaText.compare("N/A", Qt::CaseInsensitive) == 0;
            if (s.getName().trimmed().isEmpty() || !validEmailAddress(s.getEmail()) ||
                s.getCourse().trimmed().isEmpty() ||
                (!cgpaNotProvided && (!cgpaOk || cgpa < 0 || cgpa > 10))) continue;
            if (findStudent(s.getEmail()) || hasStudentRollNumber(s.getRollNo())) continue;
            if (insertStudent(s)) {
                ++added;
                logActivity("add student", s.getName() + " (" + s.getEmail() + ")");
            }
        }
        if (!db.commit()) {
            db.rollback();
            return 0;
        }
        if (added) emit changed();
        return added;
    }

    bool setStudentStatus(const QString &email, const QString &status) {
        if (!isAdminActor()) return false;
        QSqlQuery q(db);
        q.prepare("UPDATE students SET status=? WHERE email=?");
        q.addBindValue(status);
        q.addBindValue(email.trimmed());
        if (!q.exec() || q.numRowsAffected() == 0) return false;
        logActivity("update student", email + " status set to " + status);
        emit changed();
        return true;
    }

    bool shortlistCandidate(const QString &studentEmail, const QString &company, const QString &role) {
        if (!Permissions::isRecruiter(actorRoleId()) ||
            actorEmail.isEmpty()) return false;
        const auto account = actorAccount();
        const auto student = findStudentForPermission(studentEmail);
        if (!account || !student || account->extra.compare(company, Qt::CaseInsensitive) != 0 ||
            !recruiterHasStudent(student->getName())) return false;
        QSqlQuery existing(db);
        existing.prepare("UPDATE applications SET status='Shortlisted' WHERE student=? AND company=? AND role=?");
        existing.addBindValue(student->getName());
        existing.addBindValue(company);
        existing.addBindValue(role);
        if (!existing.exec()) return reportQueryError(existing, "Shortlisting candidate");
        if (existing.numRowsAffected() == 0) {
            if (!addApplication({ student->getName(), company, role, "Shortlisted" })) return false;
        } else {
            logActivity("shortlist candidate", student->getName() + " — " + company + " (" + role + ")");
            emit changed();
        }
        addNotification(studentEmail, "Application shortlisted",
                        company + " shortlisted your application for " + role + ".", "status_change");
        return true;
    }

    bool restoreStudent(const Student &student) {
        if (!isAdminActor() || findStudent(student.getEmail()) ||
            hasStudentRollNumber(student.getRollNo())) return false;
        if (!insertStudent(student)) return false;
        logActivity("restore student", student.getName() + " (" + student.getEmail() + ")");
        emit changed();
        return true;
    }

    bool removeStudent(const QString &email) {
        if (!isAdminActor()) return false;
        const auto student = findStudent(email);
        QSqlQuery q(db);
        q.prepare("DELETE FROM students WHERE email=?");
        q.addBindValue(email.trimmed());
        if (!q.exec() || q.numRowsAffected() == 0) return false;
        logActivity("delete student", student ? student->getName() + " (" + email + ")" : email);
        emit changed();
        return true;
    }

    QVector<Company> drives() const {
        QVector<Company> out;
        QSqlQuery q(db);
        const Permissions::Role role = actorRoleId();
        QString sql = "SELECT company,role,package,status,drive_date,min_cgpa,allowed_branches,approved,"
                      "job_description,eligibility,rounds "
                      "FROM drives";
        if (Permissions::isStudent(role)) sql += " WHERE approved=1";
        else if (Permissions::isRecruiter(role))
            sql += " WHERE company=(SELECT extra FROM accounts WHERE email=?)";
        else if (!Permissions::isAdmin(role)) return out;
        sql += " ORDER BY drive_date,company";
        q.prepare(sql);
        if (Permissions::isRecruiter(role)) q.addBindValue(actorEmail);
        if (!q.exec()) {
            reportQueryError(q, "Loading drives");
            return out;
        }
        while (q.next()) {
            Company drive(q.value(0).toString(), "", q.value(1).toString(), q.value(2).toString(),
                          q.value(3).toString(), q.value(4).toString(), q.value(5).toString(),
                          q.value(6).toString());
            drive.setApproved(q.value(7).toInt() != 0);
            drive.setJobDescription(q.value(8).toString());
            drive.setEligibility(q.value(9).toString());
            drive.setRounds(q.value(10).toString());
            out.append(drive);
        }
        return out;
    }

    QString eligibilityForDrive(const QString &studentEmail, const QString &company,
                                const QString &role) const {
        if (!Permissions::isStudent(actorRoleId()) ||
            actorEmail.compare(studentEmail, Qt::CaseInsensitive) != 0)
            return "Only the signed-in student can check their own placement eligibility.";
        const auto student = findStudent(studentEmail);
        if (!student) return "Student profile is unavailable.";
        const auto drive = findDrive(company, role);
        if (!drive || !drive->isApproved()) return "This drive is pending placement-office approval.";
        OfferStanding standing;
        if (!offerStanding(student->getName(), standing))
            return "Eligibility could not be checked. Contact the placement office.";
        return PlacementPolicy::eligibilityReason(*student, *drive, student->getBacklogs(),
                                                   student->getBatchYear(), standing.acceptedOffers,
                                                   2, QDate::currentDate(), standing.highestCategory);
    }

    // Eligibility for every visible drive in one pass: the student profile and the
    // offer standing are read once instead of once per drive, which is what the
    // drive grid and the recommendation card used to do.
    QHash<QString, QString> eligibilityForVisibleDrives(const QString &studentEmail) const {
        QHash<QString, QString> out;
        if (!Permissions::isStudent(actorRoleId()) ||
            actorEmail.compare(studentEmail, Qt::CaseInsensitive) != 0) return out;
        const auto student = findStudent(studentEmail);
        if (!student) return out;
        OfferStanding standing;
        if (!offerStanding(student->getName(), standing)) return out;
        const QDate today = QDate::currentDate();
        for (const Company &drive : drives()) {
            const QString key = Application::driveKey(drive.getName(), drive.getJobRole());
            out.insert(key, drive.isApproved()
                ? PlacementPolicy::eligibilityReason(*student, drive, student->getBacklogs(),
                                                     student->getBatchYear(), standing.acceptedOffers,
                                                     2, today, standing.highestCategory)
                : QStringLiteral("This drive is pending placement-office approval."));
        }
        return out;
    }

    std::optional<Company> findDrive(const QString &company, const QString &role) const {
        QSqlQuery q(db);
        q.prepare("SELECT company,role,package,status,drive_date,min_cgpa,allowed_branches,approved,"
                  "job_description,eligibility,rounds FROM drives WHERE company=? AND role=?");
        q.addBindValue(company);
        q.addBindValue(role);
        if (!q.exec()) {
            reportQueryError(q, "Finding drive");
            return std::nullopt;
        }
        if (!q.next()) return std::nullopt;
        Company drive(q.value(0).toString(), "", q.value(1).toString(), q.value(2).toString(),
                      q.value(3).toString(), q.value(4).toString(), q.value(5).toString(),
                      q.value(6).toString());
        drive.setApproved(q.value(7).toInt() != 0);
        drive.setJobDescription(q.value(8).toString());
        drive.setEligibility(q.value(9).toString());
        drive.setRounds(q.value(10).toString());
        return drive;
    }

    // The drive keys this student has already applied to, for "Applied ✓" badges
    // without one SELECT per card.
    QSet<QString> appliedDriveKeys(const QString &studentName) const {
        QSet<QString> keys;
        QSqlQuery q(db);
        q.prepare("SELECT company,role FROM applications WHERE student=?");
        q.addBindValue(studentName);
        if (!q.exec()) {
            reportQueryError(q, "Loading applied drives");
            return keys;
        }
        while (q.next())
            keys.insert(Application::driveKey(q.value(0).toString(), q.value(1).toString()));
        return keys;
    }

    bool approveDrive(const QString &company, const QString &role) {
        if (!isAdminActor()) return false;
        QSqlQuery q(db);
        q.prepare("UPDATE drives SET approved=1 WHERE company=? AND role=?");
        q.addBindValue(company);
        q.addBindValue(role);
        if (!q.exec() || q.numRowsAffected() == 0) return false;
        logActivity("approve drive", company + " — " + role);
        notifyAllStudents("Drive approved", company + " is now accepting student applications.", "new_drive");
        emit changed();
        return true;
    }

    bool verifyStudent(const QString &email) {
        if (!isAdminActor()) return false;
        QSqlQuery q(db);
        q.prepare("UPDATE students SET verified=1 WHERE email=?");
        q.addBindValue(email);
        if (!q.exec() || q.numRowsAffected() == 0) return false;
        logActivity("verify student profile", email);
        emit changed();
        return true;
    }

    bool updateStudentProfile(const Student &student) {
        if (!Permissions::isStudent(actorRoleId()) ||
            actorEmail.compare(student.getEmail(), Qt::CaseInsensitive) != 0) return false;
        QSqlQuery q(db);
        q.prepare("UPDATE students SET skills=?,projects=?,achievements=?,links=?,backlogs=?,batch_year=? "
                  "WHERE email=?");
        q.addBindValue(student.getSkills());
        q.addBindValue(student.getProjects());
        q.addBindValue(student.getAchievements());
        q.addBindValue(student.getLinks());
        q.addBindValue(student.getBacklogs());
        q.addBindValue(student.getBatchYear());
        q.addBindValue(student.getEmail());
        if (!q.exec() || q.numRowsAffected() == 0) return false;
        logActivity("update student profile", student.getEmail());
        emit changed();
        return true;
    }

    bool addStudentDocument(const QString &email, const QString &name, const QByteArray &contents) {
        if (!Permissions::isStudent(actorRoleId()) || actorEmail.compare(email, Qt::CaseInsensitive) != 0 ||
            contents.isEmpty() || contents.size() > 25 * 1024 * 1024) return false;
        QSqlQuery q(db);
        q.prepare("INSERT INTO documents(student_email,display_name,content,verified,uploaded_at) "
                  "VALUES(?,?,?,0,?)");
        q.addBindValue(email);
        q.addBindValue(QFileInfo(name).fileName());
        q.addBindValue(contents);
        q.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        if (!q.exec()) return reportQueryError(q, "Saving student document");
        logActivity("upload student document", QFileInfo(name).fileName());
        emit changed();
        return true;
    }

    QVector<VaultDocument> studentDocuments(const QString &email) const {
        if (!canReadStudent(email)) return {};
        QSqlQuery q(db);
        q.prepare("SELECT id,display_name,content,verified FROM documents "
                  "WHERE student_email=? ORDER BY id DESC");
        q.addBindValue(email);
        if (!q.exec()) {
            reportQueryError(q, "Loading student documents");
            return {};
        }
        QVector<VaultDocument> out;
        while (q.next())
            out.append({ q.value(0).toLongLong(), q.value(1).toString(), q.value(2).toByteArray(),
                         q.value(3).toInt() != 0 });
        return out;
    }

    bool verifyDocument(qint64 documentId) {
        if (!isAdminActor()) return false;
        QSqlQuery q(db);
        q.prepare("UPDATE documents SET verified=1 WHERE id=?");
        q.addBindValue(documentId);
        if (!q.exec() || q.numRowsAffected() == 0) return false;
        logActivity("verify student document", QString::number(documentId));
        emit changed();
        return true;
    }

    bool bookInterviewSlot(const QString &studentEmail, const QString &company, const QString &role,
                           const QString &eventTitle) {
        if (!Permissions::isStudent(actorRoleId()) ||
            actorEmail.compare(studentEmail, Qt::CaseInsensitive) != 0) return false;
        const auto student = findStudent(studentEmail);
        if (!student) return false;
        if (!hasApplied(student->getName(), company, role)) return false;
        QSqlQuery event(db);
        event.prepare("SELECT event_date,event_time FROM events WHERE company=? AND title=? "
                      "AND type='Interview' LIMIT 1");
        event.addBindValue(company);
        event.addBindValue(eventTitle);
        if (!event.exec() || !event.next()) return false;
        QSqlQuery conflict(db);
        conflict.prepare("SELECT 1 FROM interview_slots s JOIN events e ON e.id=s.event_id "
                         "WHERE s.student_email=? AND e.event_date=? AND e.event_time=?");
        conflict.addBindValue(studentEmail);
        conflict.addBindValue(event.value(0));
        conflict.addBindValue(event.value(1));
        if (!conflict.exec() || conflict.next()) return false;
        QSqlQuery q(db);
        q.prepare("INSERT INTO interview_slots(student_email,company,role,event_id,booked_at) "
                  "VALUES(?,?,?,(SELECT id FROM events WHERE company=? AND title=? AND type='Interview' LIMIT 1),?)");
        q.addBindValue(studentEmail);
        q.addBindValue(company);
        q.addBindValue(role);
        q.addBindValue(company);
        q.addBindValue(eventTitle);
        q.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        if (!q.exec()) return reportQueryError(q, "Booking interview slot");
        logActivity("book interview", studentEmail + " booked " + eventTitle);
        emit changed();
        return true;
    }

    bool submitDriveFeedback(const QString &studentEmail, const QString &company, const QString &role,
                             int rating, const QString &comments) {
        if (!Permissions::isStudent(actorRoleId()) ||
            actorEmail.compare(studentEmail, Qt::CaseInsensitive) != 0 ||
            rating < 1 || rating > 5 || !hasApplied(studentNameForEmail(studentEmail), company, role))
            return false;
        QSqlQuery existing(db);
        existing.prepare("SELECT 1 FROM feedback WHERE student_email=? AND company=? AND role=? LIMIT 1");
        existing.addBindValue(studentEmail);
        existing.addBindValue(company);
        existing.addBindValue(role);
        if (!existing.exec()) return reportQueryError(existing, "Checking existing drive feedback");
        if (existing.next()) return false;
        QSqlQuery q(db);
        q.prepare("INSERT INTO feedback(student_email,company,role,rating,comments,created_at) VALUES(?,?,?,?,?,?)");
        q.addBindValue(studentEmail);
        q.addBindValue(company);
        q.addBindValue(role);
        q.addBindValue(rating);
        q.addBindValue(comments.trimmed());
        q.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        if (!q.exec()) return reportQueryError(q, "Saving drive feedback");
        logActivity("submit drive feedback", company + " — " + role);
        emit changed();
        return true;
    }

    bool postAnnouncement(const QString &branch, const QString &message) {
        const Permissions::Role role = actorRoleId();
        const bool coordinator = Permissions::isCoordinator(role);
        const auto account = coordinator ? actorAccount() : std::optional<Account>();
        const QString targetBranch = branch.trimmed();
        if ((!isAdminActor() && !coordinator) || message.trimmed().isEmpty() ||
            (coordinator && (!account || targetBranch.isEmpty() ||
                             targetBranch.compare(account->extra, Qt::CaseInsensitive) != 0)))
            return false;
        QSqlQuery q(db);
        q.prepare("INSERT INTO announcements(branch,message,created_at,author) VALUES(?,?,?,?)");
        q.addBindValue(targetBranch);
        q.addBindValue(message.trimmed());
        q.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        q.addBindValue(actorEmail);
        if (!q.exec()) return reportQueryError(q, "Posting announcement");
        QSqlQuery recipients(db);
        recipients.prepare(branch.trimmed().isEmpty()
            ? "SELECT email FROM students"
            : "SELECT email FROM students WHERE course LIKE ?");
        if (!targetBranch.isEmpty()) recipients.addBindValue("%" + targetBranch + "%");
        if (!recipients.exec()) return reportQueryError(recipients, "Loading announcement recipients");
        QStringList emails;
        while (recipients.next()) emails.append(recipients.value(0).toString());
        for (const QString &email : emails)
            addNotification(email, "Placement announcement", message.trimmed(), "announcement");
        logActivity("post announcement", targetBranch.isEmpty() ? "All students" : targetBranch);
        emit changed();
        return true;
    }

    QString interviewConflictReason(const Event &event) const {
        const Permissions::Role userRole = actorRoleId();
        if ((!Permissions::isAdmin(userRole) && !Permissions::isRecruiter(userRole)) ||
            event.type.compare("Interview", Qt::CaseInsensitive) != 0) return {};
        QString companyRestriction;
        if (Permissions::isRecruiter(userRole)) {
            const auto account = actorAccount();
            if (!account || account->extra.compare(event.company, Qt::CaseInsensitive) != 0)
                return "Recruiters can schedule interviews only for their own company.";
            companyRestriction = " AND proposed.company=?";
        }
        QSqlQuery conflict(db);
        conflict.prepare(
            "SELECT DISTINCT s.name FROM events e "
            "JOIN applications existing ON existing.company=e.company "
            "JOIN applications proposed ON proposed.student=existing.student "
            "JOIN students s ON s.name=proposed.student "
            "WHERE e.event_date=? AND e.event_time=? AND lower(e.type)='interview' "
            "AND e.company<>? AND proposed.company=?" + companyRestriction + " LIMIT 5");
        conflict.addBindValue(event.date.toString("yyyy-MM-dd"));
        conflict.addBindValue(event.time);
        conflict.addBindValue(event.company);
        conflict.addBindValue(event.company);
        if (Permissions::isRecruiter(userRole)) conflict.addBindValue(event.company);
        if (!conflict.exec()) {
            reportQueryError(conflict, "Checking interview conflicts");
            return "Could not check interview conflicts. Please retry.";
        }
        QStringList names;
        while (conflict.next()) names.append(conflict.value(0).toString());
        if (names.isEmpty()) return {};
        return "Interview time conflicts for candidates: " + names.join(", ");
    }

    bool updateCompanyContact(const QString &company, const QString &contactName,
                              const QString &email, const QString &mouStatus,
                              const QString &notes) {
        if (!isAdminActor() || company.trimmed().isEmpty() ||
            (!email.trimmed().isEmpty() && !validEmailAddress(email))) return false;
        QSqlQuery q(db);
        q.prepare("INSERT INTO company_contacts(company,contact_name,contact_email,mou_status,notes) "
                  "VALUES(?,?,?,?,?) ON CONFLICT(company) DO UPDATE SET contact_name=excluded.contact_name,"
                  "contact_email=excluded.contact_email,mou_status=excluded.mou_status,notes=excluded.notes");
        q.addBindValue(company.trimmed());
        q.addBindValue(contactName.trimmed());
        q.addBindValue(email.trimmed());
        q.addBindValue(mouStatus.trimmed());
        q.addBindValue(notes.trimmed());
        if (!q.exec()) return reportQueryError(q, "Saving company contact");
        logActivity("update company contact", company);
        emit changed();
        return true;
    }

    QVector<QStringList> companyContacts() const {
        QVector<QStringList> out;
        if (!isAdminActor()) return out;
        QSqlQuery q(db);
        if (!q.exec("SELECT company,contact_name,contact_email,mou_status,notes "
                    "FROM company_contacts ORDER BY company")) {
            reportQueryError(q, "Loading company contacts");
            return out;
        }
        while (q.next())
            out.append({ q.value(0).toString(), q.value(1).toString(), q.value(2).toString(),
                         q.value(3).toString(), q.value(4).toString() });
        return out;
    }

    bool acceptOffer(const QString &studentEmail, const QString &company, const QString &role) {
        if (!Permissions::isStudent(actorRoleId()) ||
            actorEmail.compare(studentEmail, Qt::CaseInsensitive) != 0) return false;
        const QString studentName = studentNameForEmail(studentEmail);
        if (studentName.isEmpty()) return false;
        QSqlQuery existingOffer(db);
        existingOffer.prepare("SELECT COUNT(*) FROM applications WHERE student=? AND offer_decision='Accepted'");
        existingOffer.addBindValue(studentName);
        if (!existingOffer.exec() || !existingOffer.next() || existingOffer.value(0).toInt() > 0)
            return false;
        if (!db.transaction()) return false;
        QSqlQuery accepted(db);
        accepted.prepare("UPDATE applications SET offer_decision='Accepted' WHERE student=? AND company=? "
                         "AND role=? AND status LIKE '%Offer%' AND offer_decision=''");
        accepted.addBindValue(studentName);
        accepted.addBindValue(company);
        accepted.addBindValue(role);
        if (!accepted.exec() || accepted.numRowsAffected() == 0) {
            db.rollback();
            return false;
        }
        QSqlQuery closeOther(db);
        closeOther.prepare("UPDATE applications SET status='Closed - Offer Accepted' "
                           "WHERE student=? AND NOT (company=? AND role=?) AND offer_decision<>'Accepted'");
        closeOther.addBindValue(studentName);
        closeOther.addBindValue(company);
        closeOther.addBindValue(role);
        const bool closed = closeOther.exec();
        QSqlQuery placed(db);
        placed.prepare("UPDATE students SET status='Placed' WHERE email=?");
        placed.addBindValue(studentEmail);
        if (!closed || !placed.exec() || !db.commit()) {
            db.rollback();
            return false;
        }
        logActivity("accept offer", studentName + " accepted " + company + " — " + role);
        emit changed();
        return true;
    }

    bool declineOffer(const QString &studentEmail, const QString &company, const QString &role) {
        if (!Permissions::isStudent(actorRoleId()) ||
            actorEmail.compare(studentEmail, Qt::CaseInsensitive) != 0) return false;
        QSqlQuery q(db);
        q.prepare("UPDATE applications SET offer_decision='Declined' WHERE student=? AND company=? "
                  "AND role=? AND status LIKE '%Offer%'");
        q.addBindValue(studentNameForEmail(studentEmail));
        q.addBindValue(company);
        q.addBindValue(role);
        if (!q.exec() || q.numRowsAffected() == 0) return false;
        logActivity("decline offer", company + " — " + role);
        emit changed();
        return true;
    }

    bool addDrive(const Company &c) {
        const Permissions::Role role = actorRoleId();
        if (!Permissions::canManageDrives(role)) return false;
        const double package = Util::parseLpa(c.getPackageOffer());
        bool cutoffOk = false;
        const double cutoff = c.getMinCgpa().toDouble(&cutoffOk);
        if (c.getName().trimmed().isEmpty() || c.getJobRole().trimmed().isEmpty() ||
            package <= 0 || !c.date().isValid() || c.date() < QDate::currentDate() ||
            (cutoffOk && (cutoff < 0 || cutoff > 10))) return false;
        if (Permissions::isRecruiter(role)) {
            const auto account = actorAccount();
            if (!account || account->extra.compare(c.getName(), Qt::CaseInsensitive) != 0) return false;
        }
        if (!insertDrive(c)) return false;
        logActivity("schedule drive", c.getName() + " — " + c.getJobRole());
        const bool immediatelyVisible = role == Permissions::Role::None ||
                                        Permissions::isAdmin(role);
        if (immediatelyVisible)
            notifyAllStudents("New placement drive", c.getName() + " is hiring for " + c.getJobRole() + ".",
                              "new_drive");
        emit changed();
        return true;
    }

    bool setDriveStatus(const QString &company, const QString &role, const QString &status) {
        const Permissions::Role userRole = actorRoleId();
        if (!Permissions::canManageDrives(userRole)) return false;
        if (Permissions::isRecruiter(userRole)) {
            const auto account = actorAccount();
            if (!account || account->extra.compare(company, Qt::CaseInsensitive) != 0) return false;
        }
        QSqlQuery q(db);
        q.prepare("UPDATE drives SET status=? WHERE company=? AND role=?");
        q.addBindValue(status);
        q.addBindValue(company);
        q.addBindValue(role);
        if (!q.exec() || q.numRowsAffected() == 0) return false;
        logActivity("update drive", company + " — " + role + " status set to " + status);
        emit changed();
        return true;
    }

    bool removeDrive(const QString &company, const QString &role) {
        if (!Permissions::canManageDrives(actorRoleId())) return false;
        if (Permissions::isRecruiter(actorRoleId())) {
            const auto account = actorAccount();
            if (!account || account->extra.compare(company, Qt::CaseInsensitive) != 0) return false;
        }
        QSqlQuery q(db);
        q.prepare("DELETE FROM drives WHERE company=? AND role=?");
        q.addBindValue(company);
        q.addBindValue(role);
        if (!q.exec() || q.numRowsAffected() == 0) return false;
        logActivity("delete drive", company + " — " + role);
        emit changed();
        return true;
    }

    QVector<Application> applications() const {
        QVector<Application> out;
        QSqlQuery q(db);
        const Permissions::Role role = actorRoleId();
        QString sql = "SELECT student,company,role,status,scorecard,offer_decision FROM applications";
        if (Permissions::isStudent(role)) sql += " WHERE student=(SELECT name FROM students WHERE email=?)";
        else if (Permissions::isRecruiter(role))
            sql += " WHERE company=(SELECT extra FROM accounts WHERE email=?)";
        else if (Permissions::isCoordinator(role))
            sql += " WHERE student IN (SELECT name FROM students WHERE course LIKE "
                   "(SELECT '%'||extra||'%' FROM accounts WHERE email=?))";
        else if (!Permissions::isAdmin(role)) return out;
        sql += " ORDER BY student,company";
        q.prepare(sql);
        if (sql.contains('?')) q.addBindValue(actorEmail);
        if (!q.exec()) {
            reportQueryError(q, "Loading applications");
            return out;
        }
        while (q.next())
            out.append({ q.value(0).toString(), q.value(1).toString(), q.value(2).toString(),
                         q.value(3).toString(), q.value(4).toString(), q.value(5).toString() });
        return out;
    }

    bool saveInterviewScorecard(const QString &student, const QString &company,
                               const QString &role, const QString &scorecard) {
        const Permissions::Role userRole = actorRoleId();
        if (!Permissions::canManageApplications(userRole) ||
            (Permissions::isRecruiter(userRole) &&
             !recruiterOwnsApplication(student, company, role)) ||
            scorecard.trimmed().isEmpty()) return false;
        QSqlQuery q(db);
        q.prepare("UPDATE applications SET scorecard=? WHERE student=? AND company=? AND role=?");
        q.addBindValue(scorecard.trimmed());
        q.addBindValue(student);
        q.addBindValue(company);
        q.addBindValue(role);
        if (!q.exec() || q.numRowsAffected() == 0) return false;
        logActivity("save interview scorecard", student + " — " + company + " (" + role + ")");
        emit changed();
        return true;
    }

    bool canManageCompanyContact() const { return isAdminActor(); }

    bool canVerifyStudentDocuments() const { return isAdminActor(); }

    bool hasApplied(const QString &student, const QString &company, const QString &role) const {
        const Permissions::Role userRole = actorRoleId();
        if (Permissions::isStudent(userRole)) {
            const auto account = actorAccount();
            if (!account || account->name.compare(student, Qt::CaseInsensitive) != 0) return false;
        } else if (Permissions::isRecruiter(userRole) && !recruiterOwnsApplication(student, company, role))
            return false;
        QSqlQuery q(db);
        q.prepare("SELECT 1 FROM applications WHERE student=? AND company=? AND role=?");
        q.addBindValue(student);
        q.addBindValue(company);
        q.addBindValue(role);
        if (!q.exec()) {
            reportQueryError(q, "Checking application");
            return false;
        }
        return q.next();
    }

    bool addApplication(const Application &a) {
        const Permissions::Role roleName = actorRoleId();
        if (Permissions::isStudent(roleName)) {
            if (a.status != "Applied") return false;
            const auto own = findStudent(actorEmail);
            if (!own || own->getName() != a.student) return false;
            if (!eligibilityForDrive(actorEmail, a.company, a.role).isEmpty()) return false;
        } else if (Permissions::isRecruiter(roleName)) {
            if (a.status != "Applied" && a.status != "Shortlisted") return false;
            const auto account = actorAccount();
            if (!account || account->extra.compare(a.company, Qt::CaseInsensitive) != 0) return false;
        } else if (!Permissions::isAdmin(roleName)) return false;
        if (hasApplied(a.student, a.company, a.role)) return false;
        if (!insertApplication(a)) return false;
        logActivity("apply", a.student + " applied to " + a.company + " — " + a.role);
        emit changed();
        return true;
    }

    bool updateApplicationStatus(const QString &student, const QString &company, const QString &role,
                                 const QString &status) {
        if (!Permissions::canManageApplications(actorRoleId())) return false;
        if (Permissions::isRecruiter(actorRoleId()) && !recruiterOwnsApplication(student, company, role))
            return false;
        static const QStringList allowed = { "Applied", "Shortlisted", "Interview", "Offer", "Rejected" };
        if (!allowed.contains(status)) return false;
        QSqlQuery q(db);
        q.prepare("UPDATE applications SET status=?,"
                  "offer_decision=CASE WHEN ?='Offer' THEN offer_decision ELSE '' END "
                  "WHERE student=? AND company=? AND role=?");
        q.addBindValue(status);
        q.addBindValue(status);
        q.addBindValue(student);
        q.addBindValue(company);
        q.addBindValue(role);
        if (!q.exec() || q.numRowsAffected() == 0) return false;
        const QString detail = student + " — " + company + " (" + role + "): " + status;
        logActivity("application status changed", detail);
        QSqlQuery recipient(db);
        recipient.prepare("SELECT email FROM students WHERE name=? COLLATE NOCASE");
        recipient.addBindValue(student);
        if (recipient.exec())
            while (recipient.next())
                addNotification(recipient.value(0).toString(), "Application status updated",
                                company + " updated your status to " + status + ".", "status_change");
        emit changed();
        return true;
    }

    bool withdrawApplication(const QString &student, const QString &company, const QString &role) {
        if (!Permissions::isStudent(actorRoleId()) || !hasApplied(student, company, role)) return false;
        QSqlQuery q(db);
        q.prepare("DELETE FROM applications WHERE student=? AND company=? AND role=?");
        q.addBindValue(student);
        q.addBindValue(company);
        q.addBindValue(role);
        if (!q.exec() || q.numRowsAffected() == 0) return false;
        logActivity("withdraw application", student + " withdrew from " + company + " — " + role);
        emit changed();
        return true;
    }

    QVector<Event> events() const {
        QVector<Event> out;
        QSqlQuery q(db);
        const Permissions::Role role = actorRoleId();
        QString sql = "SELECT event_date,event_time,title,type,detail,company,state FROM events";
        if (Permissions::isStudent(role)) {
            sql += " WHERE title NOT LIKE 'Interview:%' OR title LIKE "
                   "'Interview: '||(SELECT name FROM students WHERE email=?)";
        } else if (Permissions::isRecruiter(role)) {
            sql += " WHERE company=(SELECT extra FROM accounts WHERE email=?)";
        } else if (Permissions::isCoordinator(role)) {
            sql += " WHERE company IN (SELECT DISTINCT a.company FROM applications a "
                   "JOIN students s ON s.name=a.student WHERE s.course LIKE "
                   "(SELECT '%'||extra||'%' FROM accounts WHERE email=?)) "
                   "AND (title NOT LIKE 'Interview:%' OR title IN "
                   "(SELECT 'Interview: '||s.name FROM students s JOIN applications a "
                   "ON a.student=s.name WHERE s.course LIKE "
                   "(SELECT '%'||extra||'%' FROM accounts WHERE email=?)))";
        } else if (!Permissions::isAdmin(role)) {
            return out;
        }
        sql += " ORDER BY event_date,event_time";
        if (!q.prepare(sql)) {
            reportQueryError(q, "Preparing calendar events");
            return out;
        }
        if (Permissions::isStudent(role) || Permissions::isRecruiter(role))
            q.addBindValue(actorEmail);
        else if (Permissions::isCoordinator(role)) {
            q.addBindValue(actorEmail);
            q.addBindValue(actorEmail);
        }
        if (!q.exec()) {
            reportQueryError(q, "Loading calendar events");
            return out;
        }
        while (q.next())
            out.append({ QDate::fromString(q.value(0).toString(), "yyyy-MM-dd"), q.value(1).toString(),
                         q.value(2).toString(), q.value(3).toString(), q.value(4).toString(),
                         q.value(5).toString(), q.value(6).toString() });
        return out;
    }

    bool addEvent(const Event &e) {
        const Permissions::Role role = actorRoleId();
        if (!Permissions::canManageDrives(role)) return false;
        if (Permissions::isRecruiter(role)) {
            const auto account = actorAccount();
            if (!account || account->extra.compare(e.company, Qt::CaseInsensitive) != 0) return false;
        }
        if (!interviewConflictReason(e).isEmpty()) return false;
        QSqlQuery q(db);
        q.prepare("INSERT INTO events(event_date,event_time,title,type,detail,company,state) VALUES(?,?,?,?,?,?,?)");
        q.addBindValue(e.date.toString("yyyy-MM-dd"));
        q.addBindValue(e.time);
        q.addBindValue(e.title);
        q.addBindValue(e.type);
        q.addBindValue(e.detail);
        q.addBindValue(e.company);
        q.addBindValue(e.state);
        if (!q.exec()) return reportQueryError(q, "Adding calendar event");
        logActivity("schedule event", e.title + " on " + e.date.toString(Qt::ISODate));
        if (e.type.compare("Interview", Qt::CaseInsensitive) == 0) {
            QSqlQuery recipients(db);
            recipients.prepare("SELECT DISTINCT s.email FROM students s JOIN applications a ON a.student=s.name "
                               "WHERE a.company=?");
            recipients.addBindValue(e.company);
            if (recipients.exec())
                while (recipients.next())
                    addNotification(recipients.value(0).toString(), "Interview scheduled",
                                    e.title + " is scheduled for " + e.date.toString("dd MMM yyyy") + " at " + e.time + ".",
                                    "interview");
        }
        emit changed();
        return true;
    }

    std::optional<Account> findAccount(const QString &email) const {
        if (actorEmail.isEmpty() ||
            (actorEmail.compare(email.trimmed(), Qt::CaseInsensitive) != 0 && !isAdminActor()))
            return std::nullopt;
        return accountByEmail(email);
    }

    std::optional<Account> authenticate(const QString &email, const QString &password) const {
        remainingLockout = loginLockoutSeconds(email);
        if (remainingLockout > 0) return std::nullopt;
        const auto account = accountByEmail(email);
        if (!account) return std::nullopt;
        const bool legacy = account->hash.startsWith("legacy-sha256:");
        const QByteArray actual = legacy
            ? QCryptographicHash::hash((account->salt + password).toUtf8(), QCryptographicHash::Sha256).toHex()
            : pbkdf2(password.toUtf8(), QByteArray::fromHex(account->salt.toLatin1()));
        const QByteArray expected = legacy
            ? account->hash.mid(QByteArray("legacy-sha256:").size()).toLatin1()
            : account->hash.toLatin1();
        if (!constantTimeEqual(actual, expected)) return std::nullopt;
        if (legacy) {
            const QString salt = QString::fromLatin1(randomSalt());
            QSqlQuery upgrade(db);
            upgrade.prepare("UPDATE accounts SET salt=?,password_hash=? WHERE email=?");
            upgrade.addBindValue(salt);
            upgrade.addBindValue(QString::fromLatin1(
                pbkdf2(password.toUtf8(), QByteArray::fromHex(salt.toLatin1()))));
            upgrade.addBindValue(email.trimmed());
            if (!upgrade.exec()) reportQueryError(upgrade, "Upgrading legacy password hash");
        }
        clearFailedLogins(email);
        return account;
    }

    QString registerAccount(const QString &email, const QString &password, const QString &role,
                            const QString &name, const QString &extra) {
        const bool privilegedRole = role == "Admin" || role == "Department Coordinator";
        if ((privilegedRole || !actorEmail.isEmpty()) && !isAdminActor())
            return "Only the placement office can create privileged or additional accounts.";
        if (actorEmail.isEmpty() && role != "Student" && role != "Recruiter")
            return "This account role can only be issued by the placement office.";
        if (accountByEmail(email)) return "An account with this e-mail already exists.";
        const QString salt = QString::fromLatin1(randomSalt());
        QSqlQuery q(db);
        q.prepare("INSERT INTO accounts(email,salt,password_hash,role,name,extra) VALUES(?,?,?,?,?,?)");
        q.addBindValue(email.trimmed().toLower());
        q.addBindValue(salt);
        q.addBindValue(QString::fromLatin1(pbkdf2(password.toUtf8(), QByteArray::fromHex(salt.toLatin1()))));
        q.addBindValue(role);
        q.addBindValue(name.trimmed());
        q.addBindValue(extra.trimmed());
        if (!q.exec()) {
            reportQueryError(q, "Registering account");
            return lastErrorText;
        }
        invalidateActorCache();
        logActivity("register account", email.trimmed().toLower() + " (" + role + ")");
        emit changed();
        return {};
    }

    bool changePassword(const QString &email, const QString &currentPassword, const QString &newPassword) {
        if (actorEmail.compare(email, Qt::CaseInsensitive) != 0) return false;
        if (!authenticate(email, currentPassword)) return false;
        const QString salt = QString::fromLatin1(randomSalt());
        QSqlQuery q(db);
        q.prepare("UPDATE accounts SET salt=?,password_hash=? WHERE email=?");
        q.addBindValue(salt);
        q.addBindValue(QString::fromLatin1(pbkdf2(newPassword.toUtf8(), QByteArray::fromHex(salt.toLatin1()))));
        q.addBindValue(email.trimmed());
        if (!q.exec() || q.numRowsAffected() == 0) return false;
        logActivity("change password", email.trimmed());
        emit changed();
        return true;
    }

    QString dataFolder() const { return QFileInfo(db.databaseName()).absolutePath(); }
    QString databasePath() const { return db.databaseName(); }

    bool changeDataFolder(const QString &folder) {
        if (!isAdminActor()) return false;
        const QString targetFolder = QDir(folder).absolutePath();
        if (!QDir().mkpath(targetFolder)) {
            lastErrorText = "Could not create data folder: " + targetFolder;
            AppLogger::write("ERROR", lastErrorText);
            return false;
        }
        const QString oldPath = db.databaseName();
        const QString newPath = targetFolder + "/placebridge.sqlite";
        if (QDir::cleanPath(oldPath) == QDir::cleanPath(newPath)) return true;
        QFile input(oldPath);
        if (!input.open(QIODevice::ReadOnly)) {
            lastErrorText = "Could not read the current database: " + input.errorString();
            AppLogger::write("ERROR", lastErrorText);
            return false;
        }
        const QByteArray contents = input.readAll();
        input.close();
        db.close();
        QSaveFile output(newPath);
        bool saved = output.open(QIODevice::WriteOnly) &&
                     output.write(contents) == contents.size() && output.commit();
        if (!saved) {
            output.cancelWriting();
            lastErrorText = "Could not write database to: " + newPath;
            AppLogger::write("ERROR", lastErrorText);
            db.setDatabaseName(oldPath);
            if (!db.open()) reportQueryError(QSqlQuery(db), "Reopening original database");
            return false;
        }
        db.setDatabaseName(newPath);
        if (!db.open()) {
            lastErrorText = "Could not open database in the selected folder: " + db.lastError().text();
            db.setDatabaseName(oldPath);
            if (!db.open()) reportQueryError(QSqlQuery(db), "Reopening original database");
            AppLogger::write("ERROR", lastErrorText);
            return false;
        }
        invalidateActorCache();
        QSettings().setValue("data/folder", targetFolder);
        logActivity("change data folder", targetFolder);
        emit changed();
        return true;
    }

    bool restoreDatabaseFromFile(const QString &sourcePath) {
        if (!isAdminActor()) return false;
        QFile input(sourcePath);
        if (!input.open(QIODevice::ReadOnly)) {
            lastErrorText = "Could not read backup database: " + input.errorString();
            AppLogger::write("ERROR", lastErrorText);
            return false;
        }
        const QByteArray contents = input.readAll();
        input.close();
        if (contents.isEmpty()) {
            lastErrorText = "The backup database is empty.";
            return false;
        }
        QTemporaryFile staged;
        if (!staged.open() || staged.write(contents) != contents.size() || !staged.flush()) {
            lastErrorText = "Could not stage the backup database for validation.";
            AppLogger::write("ERROR", lastErrorText);
            return false;
        }
        const QString stagedPath = staged.fileName();
        staged.close();
        if (!validBackupDatabase(stagedPath)) {
            lastErrorText = "The selected file is not a valid PlaceBridge database.";
            AppLogger::write("ERROR", lastErrorText);
            return false;
        }
        const QString path = db.databaseName();
        db.close();
        QSaveFile output(path);
        const bool saved = output.open(QIODevice::WriteOnly) &&
                           output.write(contents) == contents.size() && output.commit();
        db.setDatabaseName(path);
        if (!db.open()) {
            lastErrorText = "Could not reopen restored database: " + db.lastError().text();
            AppLogger::write("ERROR", lastErrorText);
            return false;
        }
        if (!createSchema()) return false;
        if (!saved) {
            lastErrorText = "Could not atomically restore database: " + output.errorString();
            AppLogger::write("ERROR", lastErrorText);
            return false;
        }
        invalidateActorCache();
        logActivity("restore database", sourcePath);
        emit changed();
        return true;
    }

    bool adminResetPassword(const QString &email, const QString &newPassword) {
        if (!isAdminActor() || !findAccount(email)) return false;
        const QString salt = QString::fromLatin1(randomSalt());
        QSqlQuery q(db);
        q.prepare("UPDATE accounts SET salt=?,password_hash=?,failed_attempts=0,locked_until=0 WHERE email=?");
        q.addBindValue(salt);
        q.addBindValue(QString::fromLatin1(pbkdf2(newPassword.toUtf8(), QByteArray::fromHex(salt.toLatin1()))));
        q.addBindValue(email.trimmed());
        if (!q.exec() || q.numRowsAffected() == 0) return false;
        logActivity("admin reset password", email.trimmed());
        emit changed();
        return true;
    }

    void logActivity(const QString &action, const QString &details) {
        if (!actorEmail.isEmpty())
            AppLogger::write("INFO", actorEmail + " | " + action + " | " + details);
        QSqlQuery q(db);
        q.prepare("INSERT INTO activity_log(actor,action,details,occurred_at) VALUES(?,?,?,?)");
        q.addBindValue(actorEmail.isEmpty() ? "system" : actorEmail);
        q.addBindValue(action);
        q.addBindValue(details);
        q.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        if (!q.exec()) reportQueryError(q, "Writing activity log");
    }

    QVector<ActivityEntry> activityLog(int limit = 500) const {
        if (!Permissions::canViewActivity(actorRoleId())) return {};
        QVector<ActivityEntry> out;
        QSqlQuery q(db);
        q.prepare("SELECT id,actor,action,details,occurred_at FROM activity_log ORDER BY id DESC LIMIT ?");
        q.addBindValue(qBound(1, limit, 5000));
        if (!q.exec()) {
            reportQueryError(q, "Loading activity log");
            return out;
        }
        while (q.next())
            out.append({ q.value(0).toLongLong(), q.value(1).toString(), q.value(2).toString(),
                         q.value(3).toString(), QDateTime::fromString(q.value(4).toString(), Qt::ISODateWithMs) });
        return out;
    }

    QVector<UserNotification> notifications(const QString &email, bool unreadOnly = false) const {
        if (actorEmail.compare(email, Qt::CaseInsensitive) != 0 && !isAdminActor()) return {};
        QVector<UserNotification> out;
        QSqlQuery q(db);
        q.prepare(QString("SELECT id,title,body,type,created_at,is_read FROM notifications "
                          "WHERE recipient_email=?%1 ORDER BY id DESC").arg(unreadOnly ? " AND is_read=0" : ""));
        q.addBindValue(email.trimmed());
        if (!q.exec()) {
            reportQueryError(q, "Loading notifications");
            return out;
        }
        while (q.next())
            out.append({ q.value(0).toLongLong(), q.value(1).toString(), q.value(2).toString(),
                         q.value(3).toString(), QDateTime::fromString(q.value(4).toString(), Qt::ISODateWithMs),
                         q.value(5).toInt() != 0 });
        return out;
    }

    int unreadNotificationCount(const QString &email) const {
        if (actorEmail.compare(email, Qt::CaseInsensitive) != 0 && !isAdminActor()) return 0;
        QSqlQuery q(db);
        q.prepare("SELECT COUNT(*) FROM notifications WHERE recipient_email=? AND is_read=0");
        q.addBindValue(email.trimmed());
        if (!q.exec()) {
            reportQueryError(q, "Counting unread notifications");
            return 0;
        }
        return q.next() ? q.value(0).toInt() : 0;
    }

    bool markNotificationsRead(const QString &email) {
        if (actorEmail.compare(email, Qt::CaseInsensitive) != 0 && !isAdminActor()) return false;
        QSqlQuery q(db);
        q.prepare("UPDATE notifications SET is_read=1 WHERE recipient_email=? AND is_read=0");
        q.addBindValue(email.trimmed());
        if (!q.exec()) return reportQueryError(q, "Marking notifications as read");
        if (q.numRowsAffected()) emit changed();
        return true;
    }

    bool resetDemoData() {
        if (!isAdminActor()) return false;
        if (!db.transaction()) return false;
        const QStringList tables = { "notifications", "activity_log", "interview_slots",
            "feedback", "documents", "announcements", "company_contacts", "events",
            "applications", "drives", "students", "accounts" };
        for (const QString &table : tables)
            if (!execSql("DELETE FROM " + table, "Resetting demo data")) {
                db.rollback();
                return false;
            }
        if (!db.commit()) {
            db.rollback();
            return false;
        }
        invalidateActorCache();
        if (!seed()) return false;
        logActivity("reset demo data", "Restored the original PlaceBridge demo dataset");
        emit changed();
        return true;
    }

    int loginLockoutSeconds(const QString &email) const {
        QSqlQuery q(db);
        q.prepare("SELECT locked_until FROM accounts WHERE email=?");
        q.addBindValue(email.trimmed());
        if (!q.exec()) {
            reportQueryError(q, "Checking login lockout");
            return 0;
        }
        if (!q.next()) return 0;
        const qint64 seconds = q.value(0).toLongLong() - QDateTime::currentSecsSinceEpoch();
        return static_cast<int>(qMax<qint64>(0, seconds));
    }

    int recordFailedLogin(const QString &email) {
        QSqlQuery q(db);
        q.prepare("UPDATE accounts SET failed_attempts=failed_attempts+1,"
                  "locked_until=CASE WHEN failed_attempts+1>=5 THEN ? ELSE locked_until END WHERE email=?");
        q.addBindValue(QDateTime::currentSecsSinceEpoch() + 60);
        q.addBindValue(email.trimmed());
        if (!q.exec()) {
            reportQueryError(q, "Recording failed login");
            return 0;
        }
        const int seconds = loginLockoutSeconds(email);
        if (seconds > 0) logActivity("login locked", email.trimmed() + " locked for 60 seconds");
        return seconds;
    }

    int remainingLoginLockout() const { return remainingLockout; }

    void clearFailedLogins(const QString &email) const {
        QSqlQuery q(db);
        q.prepare("UPDATE accounts SET failed_attempts=0,locked_until=0 WHERE email=?");
        q.addBindValue(email.trimmed());
        if (!q.exec()) reportQueryError(q, "Clearing login failures");
    }

    // ------------------------------------------------------------------
    // Password recovery without an e-mail server: the placement office issues a
    // short-lived one-time code, the user redeems it on the login screen.
    // Only a PBKDF2 hash of the code is stored, so it cannot be read back.
    // ------------------------------------------------------------------
    QString issuePasswordResetCode(const QString &email) {
        if (!Permissions::canIssueResetCodes(actorRoleId()) || !accountByEmail(email)) return {};
        static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";   // no 0/O/1/I
        QString code;
        for (int i = 0; i < 8; ++i)
            code.append(QLatin1Char(alphabet[QRandomGenerator::system()->bounded(int(sizeof(alphabet) - 1))]));
        const QString salt = QString::fromLatin1(randomSalt());
        QSqlQuery q(db);
        q.prepare("UPDATE accounts SET reset_salt=?,reset_hash=?,reset_expires=? WHERE email=?");
        q.addBindValue(salt);
        q.addBindValue(QString::fromLatin1(pbkdf2(code.toUtf8(), QByteArray::fromHex(salt.toLatin1()))));
        q.addBindValue(QDateTime::currentSecsSinceEpoch() + 15 * 60);
        q.addBindValue(email.trimmed());
        if (!q.exec()) {
            reportQueryError(q, "Issuing reset code");
            return {};
        }
        if (q.numRowsAffected() == 0) return {};
        logActivity("issue reset code", email.trimmed());
        return code;
    }

    bool redeemPasswordResetCode(const QString &email, const QString &code, const QString &newPassword) {
        if (loginLockoutSeconds(email) > 0) return false;
        QSqlQuery q(db);
        q.prepare("SELECT reset_salt,reset_hash,reset_expires FROM accounts WHERE email=?");
        q.addBindValue(email.trimmed());
        if (!q.exec() || !q.next()) return false;
        const QString resetSalt = q.value(0).toString();
        const QString resetHash = q.value(1).toString();
        const qint64 expires = q.value(2).toLongLong();
        q.finish();
        if (resetHash.isEmpty() || expires < QDateTime::currentSecsSinceEpoch()) return false;
        const QByteArray actual = pbkdf2(code.trimmed().toUpper().toUtf8(),
                                         QByteArray::fromHex(resetSalt.toLatin1()));
        if (!constantTimeEqual(actual, resetHash.toLatin1())) {
            recordFailedLogin(email);       // wrong codes count towards the normal lockout
            return false;
        }
        const QString salt = QString::fromLatin1(randomSalt());
        QSqlQuery update(db);
        update.prepare("UPDATE accounts SET salt=?,password_hash=?,failed_attempts=0,locked_until=0,"
                       "reset_salt='',reset_hash='',reset_expires=0 WHERE email=?");
        update.addBindValue(salt);
        update.addBindValue(QString::fromLatin1(pbkdf2(newPassword.toUtf8(), QByteArray::fromHex(salt.toLatin1()))));
        update.addBindValue(email.trimmed());
        if (!update.exec() || update.numRowsAffected() == 0) return false;
        AppLogger::write("INFO", email.trimmed() + " | redeem reset code | password changed");
        emit changed();
        return true;
    }

    // True while the signed-in user still has one of the documented demo passwords.
    // Only answers for the actor's own account, so it cannot be used to probe others.
    bool usesDefaultPassword(const QString &email) const {
        if (actorEmail.isEmpty() || actorEmail.compare(email.trimmed(), Qt::CaseInsensitive) != 0)
            return false;
        const auto account = accountByEmail(email);
        if (!account) return false;
        const QStringList demoPasswords = { "admin123", "student123", "recruit123" };
        if (account->hash.startsWith("legacy-sha256:")) {
            const QByteArray expected = account->hash.mid(14).toLatin1();
            for (const QString &candidate : demoPasswords) {
                const QByteArray actual = QCryptographicHash::hash((account->salt + candidate).toUtf8(),
                                                                  QCryptographicHash::Sha256).toHex();
                if (constantTimeEqual(actual, expected)) return true;
            }
            return false;
        }
        const QByteArray salt = QByteArray::fromHex(account->salt.toLatin1());
        for (const QString &candidate : demoPasswords)
            if (constantTimeEqual(pbkdf2(candidate.toUtf8(), salt), account->hash.toLatin1())) return true;
        return false;
    }

    // ------------------------------------------------------------------
    // Public landing stats (accessible without sign-in for the welcome screen)
    // ------------------------------------------------------------------
    struct PublicStats {
        int studentCount = 0;
        int partnerCount = 0;
        double averagePackage = 0.0;
        double highestPackage = 0.0;
        int driveCount = 0;
    };

    PublicStats publicStats() const {
        PublicStats s;
        QSqlQuery qs(db);
        if (qs.exec("SELECT COUNT(*) FROM students") && qs.next())
            s.studentCount = qs.value(0).toInt();

        QSqlQuery qp(db);
        if (qp.exec("SELECT COUNT(DISTINCT company), COUNT(*) FROM drives") && qp.next()) {
            s.partnerCount = qp.value(0).toInt();
            s.driveCount = qp.value(1).toInt();
        }

        QSqlQuery qpkg(db);
        if (qpkg.exec("SELECT package FROM drives")) {
            double sum = 0.0;
            int count = 0;
            while (qpkg.next()) {
                const QString pkgStr = qpkg.value(0).toString();
                double val = Util::parseLpa(pkgStr);
                if (val > 0.0) {
                    sum += val;
                    ++count;
                    if (val > s.highestPackage) s.highestPackage = val;
                }
            }
            if (count > 0) s.averagePackage = sum / count;
        }
        return s;
    }

    // ------------------------------------------------------------------
    // Reminders: notify applicants about drives and calendar events that fall
    // within the next `withinDays` days. Safe to call repeatedly - a reminder
    // with the same title is never sent to the same person twice.
    // ------------------------------------------------------------------
    int sendUpcomingReminders(int withinDays = 2) {
        if (!isAdminActor()) return 0;
        const QDate today = QDate::currentDate();
        const QDate last = today.addDays(qMax(0, withinDays));
        int sent = 0;
        const auto remind = [this, &sent](const QStringList &recipients, const QString &title, const QString &body) {
            for (const QString &email : recipients) {
                if (notificationExists(email, title)) continue;
                addNotification(email, title, body, "Reminder");
                ++sent;
            }
        };
        for (const Company &drive : drives()) {
            const QDate date = drive.date();
            if (!date.isValid() || date < today || date > last) continue;
            if (!drive.isApproved() || drive.getDriveStatus() == "Completed") continue;
            const QString when = date.toString("dd MMM yyyy");
            remind(applicantEmails(drive.getName()),
                   QString("Reminder: %1 drive on %2").arg(drive.getName(), when),
                   QString("%1 (%2) is scheduled for %3.").arg(drive.getName(), drive.getJobRole(), when));
        }
        for (const Event &event : events()) {
            if (!event.date.isValid() || event.date < today || event.date > last) continue;
            const QString state = event.state.trimmed();
            if (state.compare("Cancelled", Qt::CaseInsensitive) == 0 ||
                state.compare("Completed", Qt::CaseInsensitive) == 0) continue;
            const QString when = event.date.toString("dd MMM yyyy");
            const QString title = QString("Reminder: %1 on %2").arg(event.title, when);
            const QString body = QString("%1 at %2. %3").arg(event.title, event.time, event.detail).trimmed();
            if (event.title.startsWith("Interview: ")) {
                // A booked interview is private to one student.
                const QString email = studentEmailByName(event.title.mid(QString("Interview: ").size()));
                if (!email.isEmpty()) remind({ email }, title, body);
            } else if (!event.company.isEmpty()) {
                remind(applicantEmails(event.company), title, body);
            }
        }
        if (sent > 0) {
            logActivity("send reminders", QString::number(sent) + " reminder(s) sent");
            emit changed();
        }
        return sent;
    }

private:
    QStringList applicantEmails(const QString &company) const {
        QStringList out;
        QSqlQuery q(db);
        q.prepare("SELECT DISTINCT s.email FROM applications a JOIN students s ON s.name=a.student "
                  "WHERE a.company=?");
        q.addBindValue(company);
        if (!q.exec()) {
            reportQueryError(q, "Loading applicants");
            return out;
        }
        while (q.next()) out.append(q.value(0).toString());
        return out;
    }

    QString studentEmailByName(const QString &name) const {
        QSqlQuery q(db);
        q.prepare("SELECT email FROM students WHERE name=?");
        q.addBindValue(name);
        return q.exec() && q.next() ? q.value(0).toString() : QString();
    }

    bool notificationExists(const QString &email, const QString &title) const {
        QSqlQuery q(db);
        q.prepare("SELECT 1 FROM notifications WHERE recipient_email=? AND title=? LIMIT 1");
        q.addBindValue(email);
        q.addBindValue(title);
        return q.exec() && q.next();
    }

signals:
    void changed();
};

#include <QMap>
#include <QPair>

struct BranchStat {
    QString branch;
    int total = 0;
    int placed = 0;

    int percent() const { return total == 0 ? 0 : qRound(100.0 * placed / total); }
};

class PlacementStats {
public:
    static QVector<BranchStat> byBranch(const QVector<Student> &students);
    static QString chartLabel(const QString &branch);
    static QVector<QPair<QString, int>> placedByBranchChart(const QVector<Student> &students);
    static double averagePackage(const QVector<Company> &drives);
    static QVector<QPair<QString, int>> offersByCompany(const QVector<Application> &applications);
    static int placementRatePercent(const QVector<Student> &students);
};

#endif // PLACEBRIDGE_DATA_MANAGER_STORE_H
