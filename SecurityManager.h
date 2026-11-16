#ifndef PLACEBRIDGE_SECURITY_MANAGER_H
#define PLACEBRIDGE_SECURITY_MANAGER_H

#include <QByteArray>
#include <QDate>
#include <QString>

class Company;
class DataStore;
class Student;

class Permissions {
public:
    enum class Role { None, Admin, Coordinator, Recruiter, Student };

    static Role fromName(const QString &name);
    static QString accountRoleName(Role role);

    static bool isAdmin(Role role);
    static bool isStudent(Role role);
    static bool isRecruiter(Role role);
    static bool isCoordinator(Role role);

    static bool canManageStudents(Role role);
    static bool canManageDrives(Role role);
    static bool canManageApplications(Role role);
    static bool canViewActivity(Role role);
    static bool canReadAllStudents(Role role);
    static bool canViewReports(Role role);
    static bool canIssueResetCodes(Role role);

    static bool isAdmin(const QString &role);
    static bool isStudent(const QString &role);
    static bool isRecruiter(const QString &role);
    static bool isCoordinator(const QString &role);

    static bool canManageStudents(const QString &role);
    static bool canManageDrives(const QString &role);
    static bool canManageApplications(const QString &role);
    static bool canViewActivity(const QString &role);
    static bool canReadAllStudents(const QString &role);
    static bool canViewReports(const QString &role);
    static bool canIssueResetCodes(const QString &role);
};

class PasswordHasher {
public:
    static constexpr int rounds = 120000;

    static QByteArray randomSalt();
    static QByteArray pbkdf2(const QByteArray &password, const QByteArray &salt);
    static bool constantTimeEqual(const QByteArray &a, const QByteArray &b);
};

class Validator {
public:
    static QString requiredTextError(const QString &value, const QString &label);
    static QString emailError(const QString &email);
    static QString studentEmailError(const DataStore *store, const QString &email);
    static QString rollNumberError(const DataStore *store, const QString &rollNo);
    static QString cgpaError(const QString &value, bool required = false);
    static QString packageError(const QString &package);
    static QString newDriveDateError(const QDate &date);
    static QString passwordError(const QString &password);
};

class PlacementPolicy {
public:
    enum class Category { Regular = 0, Dream = 1, SuperDream = 2 };

    static Category categoryFor(double packageLpa);
    static QString categoryName(Category category);
    static QString eligibilityReason(const Student &student, const Company &drive,
                                     int backlogs, int batchYear, int currentOffers,
                                     int maximumOffers = 2,
                                     const QDate &today = QDate::currentDate(),
                                     int highestAcceptedCategory = -1);
};

#endif // PLACEBRIDGE_SECURITY_MANAGER_H
