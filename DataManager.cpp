#include "DataManager.h"

#include <QFileInfo>
#include <algorithm>

User::User(QString n, QString e) : name(std::move(n)), email(std::move(e)) {}

User::~User() = default;

bool User::canManageStudents() const { return Permissions::canManageStudents(accountRole()); }
bool User::canManageDrives() const { return Permissions::canManageDrives(accountRole()); }
bool User::canManageApplications() const { return Permissions::canManageApplications(accountRole()); }
bool User::canViewActivity() const { return Permissions::canViewActivity(accountRole()); }
bool User::canReadAllStudents() const { return Permissions::canReadAllStudents(accountRole()); }
bool User::canViewReports() const { return Permissions::canViewReports(accountRole()); }

QString User::welcome() const {
    return "Welcome, " + name + " (" + role() + ")!";
}

QString User::getName() const { return name; }
void User::setName(const QString &n) { name = n; }
QString User::getEmail() const { return email; }
void User::setEmail(const QString &e) { email = e; }

QString FileRepository::locateDataDir() {
    const QString cwd = QDir::currentPath();
    const QString app = QCoreApplication::applicationDirPath();
    const QStringList candidates = { cwd + "/data", cwd + "/../data",
                                     app + "/data", app + "/../data", app + "/../../data" };
    for (const QString &candidate : candidates) {
        if (QDir(candidate).exists()) return QDir(candidate).absolutePath();
    }
    const QString created = cwd + "/data";
    QDir().mkpath(created);
    return created;
}

FileRepository::FileRepository(const QString &filename)
    : filePath(locateDataDir() + "/" + filename) {
    if (!QFile::exists(filePath)) {
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) file.close();
    }
}

QString FileRepository::dataDirectory() { return locateDataDir(); }

QString FileRepository::clean(QString value) {
    value.replace('|', '/');
    value.replace('\n', ' ');
    value.replace('\r', ' ');
    return value.trimmed();
}

QString FileRepository::getFilePath() const { return filePath; }

QStringList FileRepository::rows() const {
    if (!loaded) {
        cache.clear();
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            while (!in.atEnd()) {
                const QString line = in.readLine().trimmed();
                if (!line.isEmpty()) cache.append(line);
            }
        }
        loaded = true;
    }
    return cache;
}

bool FileRepository::append(const QString &line) {
    QFile file(filePath);
    if (!file.open(QIODevice::Append | QIODevice::Text)) return false;
    QTextStream out(&file);
    out << line << '\n';
    if (loaded) cache.append(line.trimmed());
    return true;
}

bool FileRepository::overwrite(const QStringList &lines) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) return false;
    QTextStream out(&file);
    for (const QString &line : lines) out << line << '\n';
    cache = lines;
    loaded = true;
    return true;
}

int FileRepository::count() const { return rows().size(); }

QVector<BranchStat> PlacementStats::byBranch(const QVector<Student> &students) {
    QMap<QString, BranchStat> grouped;
    for (const Student &student : students) {
        const QString branch = student.branch().trimmed().isEmpty()
                                   ? QStringLiteral("Unassigned") : student.branch().trimmed();
        BranchStat &stat = grouped[branch];
        stat.branch = branch;
        ++stat.total;
        if (student.isPlaced()) ++stat.placed;
    }
    QVector<BranchStat> result;
    for (auto it = grouped.cbegin(); it != grouped.cend(); ++it) result.append(it.value());
    return result;
}

QString PlacementStats::chartLabel(const QString &branch) {
    if (branch.size() <= 11) return branch;
    QString initials;
    for (const QString &word : branch.split(' ', Qt::SkipEmptyParts))
        if (word.at(0).isLetter()) initials.append(word.at(0).toUpper());
    return initials.isEmpty() ? branch.left(11) : initials;
}

QVector<QPair<QString, int>> PlacementStats::placedByBranchChart(const QVector<Student> &students) {
    QVector<QPair<QString, int>> result;
    for (const BranchStat &stat : byBranch(students))
        result.append({ chartLabel(stat.branch), stat.placed });
    return result;
}

double PlacementStats::averagePackage(const QVector<Company> &drives) {
    double sum = 0;
    int count = 0;
    for (const Company &drive : drives) {
        if (drive.lpa() <= 0) continue;
        sum += drive.lpa();
        ++count;
    }
    return count == 0 ? 0.0 : sum / count;
}

QVector<QPair<QString, int>> PlacementStats::offersByCompany(const QVector<Application> &applications) {
    QMap<QString, int> counts;
    for (const Application &application : applications)
        if (application.stage() >= 3) ++counts[application.company];
    QVector<QPair<QString, int>> result;
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) result.append({ it.key(), it.value() });
    std::stable_sort(result.begin(), result.end(),
                     [](const auto &a, const auto &b) { return a.second > b.second; });
    return result;
}

int PlacementStats::placementRatePercent(const QVector<Student> &students) {
    if (students.isEmpty()) return 0;
    int placed = 0;
    for (const Student &student : students)
        if (student.isPlaced()) ++placed;
    return qRound(100.0 * placed / students.size());
}
