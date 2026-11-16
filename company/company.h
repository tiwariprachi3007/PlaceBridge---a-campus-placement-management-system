#ifndef COMPANY_H
#define COMPANY_H

// Request only the base types to break the DataStore/domain-model include cycle.
#define PLACEBRIDGE_DATA_MANAGER_TYPES_ONLY
#include "DataManager.h"
#undef PLACEBRIDGE_DATA_MANAGER_TYPES_ONLY
#include "SystemServices.h"
#include <QString>
#include <QStringList>
#include <QDate>

// ============================================================================
// OOP CONCEPT: INHERITANCE + ENCAPSULATION - Company IS-A User
// One Company object = one recruitment drive of that company.
// Record format: Company|Role|Package|Status|Date(yyyy-MM-dd)|MinCGPA
// ============================================================================                              //abstraction:implements the pure virtual function inherited from the user  
class Company : public User {                                                                                //inheritance :company inherit properties and function from user data
private:                                                                                                     //encapsulation:data members are private and accessed through public getter and setter functions 
    QString jobRole;
    QString packageOffer;
    QString driveStatus;   // Open | Scheduled | Completed
    QString driveDate;
    QString minCgpa;
    QString allowedBranches;
    QString jobDescription;
    QString eligibility;
    QString rounds;
    bool approved = true;

public:
    Company(QString companyName, QString email, QString jobRole, QString packageOffer,                         //constructor:initializes company objects with company and recruitment drive details
            QString driveStatus = "Open", QString driveDate = "", QString minCgpa = "",
            QString allowedBranches = "", QString jobDescription = "",
            QString eligibility = "", QString rounds = "")
        : User(std::move(companyName), std::move(email)),
          jobRole(std::move(jobRole)), packageOffer(std::move(packageOffer)),
          driveStatus(std::move(driveStatus)), driveDate(std::move(driveDate)),
          minCgpa(std::move(minCgpa)), allowedBranches(std::move(allowedBranches)),
          jobDescription(std::move(jobDescription)), eligibility(std::move(eligibility)),
          rounds(std::move(rounds)) {}

    QString role() const override { return "Company"; }                                                        //runtime polymorphism : overrides virtual function inherited from user class 
    Permissions::Role accountRole() const override { return Permissions::Role::Recruiter; }                     //the override keyword confirms the derived class is overriding a virtual function from its base class

    QString getJobRole() const { return jobRole; }                                                              // getter and setters:provide controlled access to private data members       
    void setJobRole(const QString &r) { jobRole = r; }
    QString getPackageOffer() const { return packageOffer; }
    void setPackageOffer(const QString &p) { packageOffer = p; }
    QString getDriveStatus() const { return driveStatus; }
    void setDriveStatus(const QString &s) { driveStatus = s; }
    QString getDriveDate() const { return driveDate; }
    void setDriveDate(const QString &d) { driveDate = d; }
    QString getMinCgpa() const { return minCgpa; }
    void setMinCgpa(const QString &m) { minCgpa = m; }
    QString getAllowedBranches() const { return allowedBranches; }
    void setAllowedBranches(const QString &branches) { allowedBranches = branches; }
    QString getJobDescription() const { return jobDescription; }
    void setJobDescription(const QString &value) { jobDescription = value; }
    QString getEligibility() const { return eligibility; }
    void setEligibility(const QString &value) { eligibility = value; }
    QString getRounds() const { return rounds; }
    void setRounds(const QString &value) { rounds = value; }
    bool isApproved() const { return approved; }
    void setApproved(bool value) { approved = value; }

    bool allowsBranch(const QString &branch) const {
        if (allowedBranches.trimmed().isEmpty()) return true;
        const QStringList allowed = allowedBranches.split(',', Qt::SkipEmptyParts);
        for (const QString &item : allowed)
            if (item.trimmed().compare(branch.trimmed(), Qt::CaseInsensitive) == 0) return true;
        return false;
    }

    double lpa() const { return Util::parseLpa(packageOffer); }
    double minCgpaValue() const { bool ok = false; double v = minCgpa.toDouble(&ok); return ok ? v : 0.0; }
    QDate date() const { return QDate::fromString(driveDate, "yyyy-MM-dd"); }
    QString initials() const { return Util::initialsOf(name); }

    QString toRecord() const {
        const QStringList f = { name, jobRole, packageOffer, driveStatus, driveDate, minCgpa };
        QStringList clean;
        for (const QString &x : f) clean << FileRepository::clean(x);
        return clean.join(QLatin1Char('|'));
    }

    static Company fromRecord(const QString &line) {                                                              //static function:creates and returns a company object without needing an instance of the class
        QStringList p = line.split('|');
        while (p.size() < 6) p << "";
        QString status = p[3].isEmpty() ? "Open" : p[3];
        if (status == "Closed") status = "Completed";   // legacy value
        return Company(p[0], "", p[1], p[2], status, p[4], p[5]);
    }
};

#endif // COMPANY_H
