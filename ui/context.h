#ifndef CONTEXT_H
#define CONTEXT_H

#include "DataManager.h"
#include <functional>
#include <QMap>

// What every page gets from the main window.
struct Ctx {
    DataStore *store;
    User *user;                                       // the signed-in user (polymorphic)
    std::function<void(const QString &)> navigate;    // switch tab by title
    std::function<void(bool)> setNavigationVisible;
};

namespace Fmt {

inline QString lpa(double v) { return QString::number(v, 'f', 1) + " LPA"; }
inline QString dateLong(const QDate &d) { return d.isValid() ? d.toString("dd MMM yyyy") : QString("TBA"); }

inline QString sectorOf(const QString &company) {
    static const QMap<QString, QString> m = {
        { "Google", "Product Engineering & Cloud" }, { "Microsoft", "Cloud Solutions & AI" },
        { "Amazon", "Systems Development" },         { "Infosys", "IT services & Digital" },
        { "TCS Digital", "IT services & Digital" },  { "Accenture", "Consulting & Technology" },
        { "L&T Technology", "Core Systems Engineering" } };
    return m.value(company, "Campus Recruiter");
}

// Coarse group used by the filter chips on the Companies page
inline QString sectorGroup(const QString &company) {
    const QString s = sectorOf(company);
    if (s.contains("IT services")) return "IT services";
    if (s.contains("Consulting")) return "Consulting";
    if (s.contains("Core")) return "Core";
    return "Product";
}

} // namespace Fmt

#endif // CONTEXT_H
