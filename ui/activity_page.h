#ifndef ACTIVITY_PAGE_H
#define ACTIVITY_PAGE_H

#include "context.h"
#include "theme.h"

class ActivityPage : public Theme::Page {
    Ctx ctx;
public:
    explicit ActivityPage(const Ctx &context);
};

#endif // ACTIVITY_PAGE_H
