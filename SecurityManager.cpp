#include "SecurityManager.h"

#include "DataManager.h"
#include "SystemServices.h"

#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QRegularExpression>

Permissions::Role Permissions::fromName(const QString &name) {
    const QString value = name.trimmed();
    if (value == "Admin" || value == "Placement Officer") return Role::Admin;
    if (value == "Department Coordinator") return Role::Coordinator;
    if (value == "Recruiter" || value == "Company") return Role::Recruiter;
    if (value == "Student") return Role::Student;
    return Role::None;
}

QString Permissions::accountRoleName(Role role) {
    switch (role) {
    case Role::Admin: return "Admin";
    case Role::Coordinator: return "Department Coordinator";
    case Role::Recruiter: return "Recruiter";
    case Role::Student: return "Student";
    default: return {};
    }
}

bool Permissions::isAdmin(Role role) { return role == Role::Admin; }
bool Permissions::isStudent(Role role) { return role == Role::Student; }
bool Permissions::isRecruiter(Role role) { return role == Role::Recruiter; }
bool Permissions::isCoordinator(Role role) { return role == Role::Coordinator; }

bool Permissions::canManageStudents(Role role) { return isAdmin(role); }
bool Permissions::canManageDrives(Role role) { return isAdmin(role) || isRecruiter(role); }
bool Permissions::canManageApplications(Role role) { return isAdmin(role) || isRecruiter(role); }
bool Permissions::canViewActivity(Role role) { return isAdmin(role); }
bool Permissions::canReadAllStudents(Role role) { return isAdmin(role) || isCoordinator(role); }
bool Permissions::canViewReports(Role role) { return isAdmin(role); }
bool Permissions::canIssueResetCodes(Role role) { return isAdmin(role); }

bool Permissions::isAdmin(const QString &role) { return isAdmin(fromName(role)); }
bool Permissions::isStudent(const QString &role) { return isStudent(fromName(role)); }
bool Permissions::isRecruiter(const QString &role) { return isRecruiter(fromName(role)); }
bool Permissions::isCoordinator(const QString &role) { return isCoordinator(fromName(role)); }

bool Permissions::canManageStudents(const QString &role) { return canManageStudents(fromName(role)); }
bool Permissions::canManageDrives(const QString &role) { return canManageDrives(fromName(role)); }
bool Permissions::canManageApplications(const QString &role) { return canManageApplications(fromName(role)); }
bool Permissions::canViewActivity(const QString &role) { return canViewActivity(fromName(role)); }
bool Permissions::canReadAllStudents(const QString &role) { return canReadAllStudents(fromName(role)); }
bool Permissions::canViewReports(const QString &role) { return canViewReports(fromName(role)); }
bool Permissions::canIssueResetCodes(const QString &role) { return canIssueResetCodes(fromName(role)); }

QByteArray PasswordHasher::randomSalt() {
    QByteArray salt(16, Qt::Uninitialized);
    for (char &byte : salt)
        byte = static_cast<char>(QRandomGenerator::system()->generate() & 0xff);
    return salt.toHex();
}

QByteArray PasswordHasher::pbkdf2(const QByteArray &password, const QByteArray &salt) {
    // One QMessageAuthenticationCode, reset between rounds, instead of the static
    // hash() helper: the key schedule is set up once rather than 120 000 times and
    // the per-round QByteArray allocations disappear. Output is unchanged.
    QByteArray block = salt;
    block.append("\0\0\0\1", 4);
    QMessageAuthenticationCode mac(QCryptographicHash::Sha256, password);
    mac.addData(block);
    QByteArray u = mac.result();
    QByteArray result = u;
    char *out = result.data();
    for (int i = 1; i < rounds; ++i) {
        mac.reset();
        mac.addData(u);
        u = mac.result();
        const char *current = u.constData();
        for (qsizetype j = 0; j < result.size(); ++j)
            out[j] = static_cast<char>(out[j] ^ current[j]);
    }
    return result.toHex();
}

bool PasswordHasher::constantTimeEqual(const QByteArray &a, const QByteArray &b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (qsizetype i = 0; i < a.size(); ++i)
        diff |= static_cast<unsigned char>(a.at(i) ^ b.at(i));
    return diff == 0;
}

QString Validator::requiredTextError(const QString &value, const QString &label) {
    return value.trimmed().isEmpty() ? "Please enter " + label + "." : QString();
}

QString Validator::emailError(const QString &email) {
    static const QRegularExpression pattern(
        "^[A-Z0-9.!#$%&'*+/=?^_`{|}~-]+@[A-Z0-9](?:[A-Z0-9-]{0,61}[A-Z0-9])?"
        "(?:\\.[A-Z0-9](?:[A-Z0-9-]{0,61}[A-Z0-9])?)+$",
        QRegularExpression::CaseInsensitiveOption);
    return pattern.match(email.trimmed()).hasMatch() ? QString() : "Please enter a valid e-mail address.";
}

QString Validator::studentEmailError(const DataStore *store, const QString &email) {
    const QString invalid = emailError(email);
    if (!invalid.isEmpty()) return invalid;
    if (store && (store->findStudent(email) || store->findAccount(email)))
        return "An account or student with this e-mail already exists.";
    return {};
}

QString Validator::rollNumberError(const DataStore *store, const QString &rollNo) {
    if (!store || rollNo.trimmed().isEmpty()) return {};
    return store->hasStudentRollNumber(rollNo) ? "This roll number is already registered." : QString();
}

QString Validator::cgpaError(const QString &value, bool required) {
    const QString text = value.trimmed();
    if (text.isEmpty()) return required ? "CGPA is required." : QString();
    bool ok = false;
    const double cgpa = text.toDouble(&ok);
    if (!ok || cgpa < 0.0 || cgpa > 10.0) return "CGPA must be a number between 0 and 10.";
    return {};
}

QString Validator::packageError(const QString &package) {
    static const QRegularExpression pattern(
        "^\\s*[0-9]+(?:\\.[0-9]+)?\\s*(?:LPA|Lakhs?(?:\\s+per\\s+annum)?)?\\s*$",
        QRegularExpression::CaseInsensitiveOption);
    return pattern.match(package).hasMatch() && Util::parseLpa(package) > 0
               ? QString() : "Enter a positive package amount, e.g. 12 LPA.";
}

QString Validator::newDriveDateError(const QDate &date) {
    return !date.isValid() ? "Please enter a valid drive date."
         : date < QDate::currentDate() ? "A new drive cannot be scheduled in the past."
                                       : QString();
}

QString Validator::passwordError(const QString &password) {
    return password.size() < 8 ? "Choose a password of at least 8 characters." : QString();
}

PlacementPolicy::Category PlacementPolicy::categoryFor(double packageLpa) {
    if (packageLpa > 12.0) return Category::SuperDream;
    if (packageLpa >= 8.0) return Category::Dream;
    return Category::Regular;
}

QString PlacementPolicy::categoryName(Category category) {
    switch (category) {
    case Category::Dream: return "Dream";
    case Category::SuperDream: return "Super Dream";
    default: return "Regular";
    }
}

QString PlacementPolicy::eligibilityReason(const Student &student, const Company &drive,
                                           int backlogs, int batchYear, int currentOffers,
                                           int maximumOffers, const QDate &today,
                                           int highestAcceptedCategory) {
    bool ok = false;
    const double cutoff = drive.getMinCgpa().toDouble(&ok);
    if (ok && student.cgpaValue() < cutoff)
        return QString("CGPA %1 is below the drive cutoff of %2.")
            .arg(student.getCgpa(), drive.getMinCgpa());
    if (backlogs > 0) return "Students with active backlogs are not eligible.";
    if (!drive.allowsBranch(student.branch())) return "Your branch is not eligible for this drive.";
    if (batchYear > 0 && today.year() - batchYear > 4)
        return "This drive is limited to current graduating batches.";
    if (currentOffers >= maximumOffers)
        return QString("Placement policy permits at most %1 accepted offer(s).").arg(maximumOffers);
    const Category target = categoryFor(drive.lpa());
    if (highestAcceptedCategory >= 0 &&
        static_cast<int>(target) < highestAcceptedCategory)
        return QString("You have accepted a higher-category offer; %1 drives are locked.")
            .arg(categoryName(target));
    return {};
}
