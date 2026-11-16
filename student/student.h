#ifndef STUDENT_H
#define STUDENT_H

// Request only the base types to break the DataStore/domain-model include cycle.
#define PLACEBRIDGE_DATA_MANAGER_TYPES_ONLY
#include "DataManager.h"
#undef PLACEBRIDGE_DATA_MANAGER_TYPES_ONLY
#include "SystemServices.h"
#include <QString>
#include <QStringList>

// ============================================================================
// OOP CONCEPT: INHERITANCE + ENCAPSULATION - Student IS-A User
// Record format: Name|Email|Course|CGPA|Status|RollNo|Skills|TestScore
// (older 4- and 5-field records are still read correctly)
// ============================================================================
class Student : public User {
private:
    QString course;
    QString cgpa;
    QString status;
    QString rollNo;
    QString skills;      // comma separated
    QString testScore;   // 0..100
    int backlogs = 0;
    int batchYear = 0;
    bool verified = false;
    QString projects;
    QString achievements;
    QString links;

public:
    Student(QString name, QString email, QString course, QString cgpa = "N/A",
            QString status = "Registered", QString rollNo = "", QString skills = "",
            QString testScore = "")
        : User(std::move(name), std::move(email)),
          course(std::move(course)), cgpa(std::move(cgpa)), status(std::move(status)),
          rollNo(std::move(rollNo)), skills(std::move(skills)), testScore(std::move(testScore)) {}

    // POLYMORPHISM: overrides the pure virtual method of User
    QString role() const override { return "Student"; }
    Permissions::Role accountRole() const override { return Permissions::Role::Student; }

    QString getCourse() const { return course; }
    void setCourse(const QString &c) { course = c; }
    QString getCgpa() const { return cgpa; }
    void setCgpa(const QString &g) { cgpa = g; }
    QString getStatus() const { return status; }
    void setStatus(const QString &s) { status = s; }
    QString getRollNo() const { return rollNo; }
    void setRollNo(const QString &r) { rollNo = r; }
    QString getSkills() const { return skills; }
    void setSkills(const QString &s) { skills = s; }
    QString getTestScore() const { return testScore; }
    void setTestScore(const QString &t) { testScore = t; }

    double cgpaValue() const { bool ok = false; double v = cgpa.toDouble(&ok); return ok ? v : -1.0; }
    int testValue() const { bool ok = false; int v = testScore.toInt(&ok); return ok ? v : -1; }
    int getBacklogs() const { return backlogs; }
    void setBacklogs(int value) { backlogs = qMax(0, value); }
    int getBatchYear() const { return batchYear; }
    void setBatchYear(int value) { batchYear = value; }
    bool isVerified() const { return verified; }
    void setVerified(bool value) { verified = value; }
    QString getProjects() const { return projects; }
    void setProjects(const QString &value) { projects = value; }
    QString getAchievements() const { return achievements; }
    void setAchievements(const QString &value) { achievements = value; }
    QString getLinks() const { return links; }
    void setLinks(const QString &value) { links = value; }
    QString branch() const { return course.startsWith("B.Tech ") ? course.mid(7) : course; }
    QString initials() const { return Util::initialsOf(name); }
    bool isPlaced() const {
        return status.startsWith("Placed", Qt::CaseInsensitive) ||
               status.startsWith("Selected", Qt::CaseInsensitive);
    }

    QStringList skillList() const {
        QStringList out;
        for (const QString &s : skills.split(',', Qt::SkipEmptyParts)) out << s.trimmed();
        return out;
    }

    QString toRecord() const {
        const QStringList f = { name, email, course, cgpa, status, rollNo, skills, testScore };
        QStringList clean;
        for (const QString &x : f) clean << FileRepository::clean(x);
        return clean.join(QLatin1Char('|'));
    }

    static Student fromRecord(const QString &line) {
        QStringList p = line.split('|');
        if (p.size() >= 8) return Student(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
        if (p.size() == 5) return Student(p[0], p[1], p[2], p[3], p[4]);
        if (p.size() == 4) return Student(p[0], p[1], p[2], "N/A", p[3]);
        while (p.size() < 3) p << "";
        return Student(p[0], p[1], p[2]);
    }
};

#endif // STUDENT_H
