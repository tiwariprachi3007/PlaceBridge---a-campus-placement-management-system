#ifndef SETTINGS_PAGE_H
#define SETTINGS_PAGE_H

#include "context.h"
#include "theme.h"

class SettingsPage : public Theme::Page {
    Q_OBJECT
    Ctx ctx;
public:
    explicit SettingsPage(const Ctx &context);
};

#endif // SETTINGS_PAGE_H
