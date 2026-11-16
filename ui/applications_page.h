#ifndef APPLICATIONS_PAGE_H
#define APPLICATIONS_PAGE_H

#include "context.h"
#include "theme.h"

class ApplicationsPage : public Theme::Page {
    Ctx ctx;
public:
    explicit ApplicationsPage(const Ctx &context);
};

#endif // APPLICATIONS_PAGE_H
