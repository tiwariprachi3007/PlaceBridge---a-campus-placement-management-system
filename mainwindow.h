#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QInputDialog>
#include <QCloseEvent>
#include <memory>

#include "theme.h"
#include "context.h"
#include "login_page.h"
#include "dashboard_page.h"
#include "drives_page.h"
#include "students_page.h"
#include "calendar_page.h"
#include "reports_page.h"
#include "student_portal.h"
#include "recruiter_page.h"
#include "placement_officer.h"
#include "applications_page.h"
#include "activity_page.h"
#include "dialogs.h"
#include "settings_page.h"
#include "search_dialog.h"
#include "resume_studio.h"

// ============================================================================
// Application shell: login -> role based workspace.
//
// OOP CONCEPTS
//  * POLYMORPHISM : `session` is a std::unique_ptr<User>; it holds a
//                   PlacementOfficer, Student or Company depending on login.
//  * COMPOSITION  : MainWindow OWNS the DataStore and the pages.
//  * ENCAPSULATION: pages only get a Ctx (store + user + navigate callback).
// ============================================================================
class MainWindow : public QMainWindow {
    Q_OBJECT

    DataStore store;
    QStackedWidget *root = nullptr;
    LoginPage *login = nullptr;
    QWidget *shell = nullptr;
    QStackedWidget *pages = nullptr;
    QStringList navTitles;
    QVector<QPushButton *> navButtons;
    QVector<bool> built;          // which tabs hold an up-to-date page
    std::unique_ptr<User> session;
    bool rebuildQueued = false;
    QPushButton *notificationButton = nullptr;
    QVector<QPushButton *> navigation;
    QLabel *rolePillLabel = nullptr;
    QLabel *userNameLabel = nullptr;
    QLabel *brandSubLabel = nullptr;
    QLabel *avatarLabel = nullptr;
    QPushButton *accountMenuButton = nullptr;
    QPushButton *themeButton = nullptr;
    QPushButton *searchButtonWidget = nullptr;
    QLabel *pageTitleLabel = nullptr;
    QFrame *sidebarFrame = nullptr;
    static constexpr int sidebarWidth = 208;
    QScrollArea *navigationScrollArea = nullptr;
    QHBoxLayout *topBarLayout = nullptr;
    QVBoxLayout *headerLayout = nullptr;
    int unreadNotifications = 0;   // cached: resizeEvent must not hit the database

public:
    MainWindow() {
        setWindowTitle("PlaceBridge | Campus Placement Management Portal");
        setWindowIcon(Theme::appIcon());
        resize(1360, 880);
        setMinimumSize(320, 480);

        root = new QStackedWidget(this);
        setCentralWidget(root);
        login = new LoginPage(&store);
        root->addWidget(login);
        connect(login, &LoginPage::authenticated, this, &MainWindow::startSession);

        // Any data change marks every tab stale and refreshes only the visible one
        // (deferred, so a button is never destroyed inside its own click handler).
        // Other tabs are rebuilt lazily the next time they are opened.
        connect(&store, &DataStore::changed, this, [this]() {
            refreshNotificationCount();
            if (rebuildQueued || !pages) return;
            rebuildQueued = true;
            QTimer::singleShot(0, this, [this]() {
                rebuildQueued = false;
                if (!pages) return;
                built.fill(false);
                refreshPage(pages->currentIndex());
            });
        });
    }

private:
    void startSession(const QString &email) {
        const auto acc = store.findAccount(email);
        if (!acc) return;
        store.setActor(acc->email);
        if (Permissions::isAdmin(acc->role)) store.sendUpcomingReminders(2);   // idempotent

        // POLYMORPHISM: three different classes behind one base pointer
        if (acc->role == "Admin") {
            session = std::make_unique<PlacementOfficer>(acc->name, acc->email, acc->extra.isEmpty() ? "PO-2024" : acc->extra);
        } else if (acc->role == "Department Coordinator") {
            session = std::make_unique<DepartmentCoordinator>(acc->name, acc->email, acc->extra);
        } else if (acc->role == "Student") {
            const auto s = store.findStudent(acc->email);
            session = std::make_unique<Student>(s ? *s : Student(acc->name, acc->email, acc->extra));
        } else {
            session = std::make_unique<Company>(acc->extra.isEmpty() ? acc->name : acc->extra, acc->email, "", "", "Open");
        }

        if (shell) { root->removeWidget(shell); shell->deleteLater(); }
        shell = buildShell(acc->name);
        root->addWidget(shell);
        root->setCurrentWidget(shell);
        for (int i = 0; i < navTitles.size(); ++i) pages->addWidget(new QWidget);   // placeholders
        built = QVector<bool>(navTitles.size(), false);
        QSettings settings;
        const QString roleKey = "window/" + session->role() + "/";
        const QSize savedSize = settings.value(roleKey + "size").toSize();
        const QPoint savedPosition = settings.value(roleKey + "position").toPoint();
        if (savedSize.isValid()) resize(savedSize);
        if (!savedPosition.isNull()) move(savedPosition);
        const QString lastTab = settings.value("navigation/" + session->role() + "/lastTab",
                                               navTitles.value(0)).toString();
        navigate(navTitles.contains(lastTab) ? lastTab : navTitles.value(0));
        QTimer::singleShot(0, this, [this]() { offerPasswordChange(); });
    }

    QWidget *buildShell(const QString &displayName) {
        auto *w = new QWidget;
        auto *v = new QVBoxLayout(w);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(0);

        // ------------------------------------------------------------------
        // Shell: a fixed left sidebar for navigation, a slim top bar for the
        // page title and account controls, and the page stack beside them.
        // The previous shell stacked a dark brand bar over a horizontal strip of
        // tab buttons, which scrolled sideways once a role had nine tabs.
        // ------------------------------------------------------------------
        auto *row = new QHBoxLayout;
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(0);

        // ---- navigation depends on the role
        navTitles.clear();
        navButtons.clear();
        navigation.clear();
        if (dynamic_cast<PlacementOfficer *>(session.get()))
            navTitles = { "Dashboard", "Drives", "Calendar", "Students", "Companies",
                          "Applications", "Reports", "Activity", "Settings" };
        else if (dynamic_cast<DepartmentCoordinator *>(session.get()))
            navTitles = { "Dashboard", "Students", "Settings" };
        else if (dynamic_cast<Student *>(session.get()))
            navTitles = { "My Profile", "Resume Studio", "Drives", "Calendar", "Settings" };
        else
            navTitles = { "Hiring Suite", "Calendar", "Settings" };

        auto *sidebar = new QFrame;
        sidebar->setObjectName("sidebar");
        sidebar->setAttribute(Qt::WA_StyledBackground, true);
        sidebar->setFixedWidth(sidebarWidth);
        sidebarFrame = sidebar;
        auto *sideLayout = new QVBoxLayout(sidebar);
        sideLayout->setContentsMargins(12, 16, 12, 12);
        sideLayout->setSpacing(4);

        auto *brandRow = new QHBoxLayout;
        brandRow->setSpacing(9);
        brandRow->addWidget(Theme::logo("P", true, 30));
        auto *brand = new QVBoxLayout;
        brand->setSpacing(0);
        auto *bn = new QLabel("PlaceBridge");
        bn->setObjectName("brandName");
        const QString sub = dynamic_cast<PlacementOfficer *>(session.get()) ? "PLACEMENT OFFICE"
                          : dynamic_cast<DepartmentCoordinator *>(session.get()) ? "DEPT COORDINATOR"
                          : dynamic_cast<Student *>(session.get())          ? "STUDENT PORTAL"
                                                                            : "RECRUITER SUITE";
        brandSubLabel = new QLabel(sub);
        brandSubLabel->setObjectName("brandSub");
        brand->addWidget(bn);
        brand->addWidget(brandSubLabel);
        brandRow->addLayout(brand, 1);
        sideLayout->addLayout(brandRow);
        sideLayout->addSpacing(14);

        auto *sectionLabel = new QLabel("WORKSPACE");
        sectionLabel->setObjectName("sideSection");
        sideLayout->addWidget(sectionLabel);

        auto *group = new QButtonGroup(w);
        group->setExclusive(true);
        for (const QString &t : navTitles) {
            // "Settings" sits at the bottom of the sidebar, away from the
            // workspace pages, as it does in every tool this borrows from.
            if (t == "Settings") continue;
            auto *b = Theme::button(t, "side");
            b->setMinimumHeight(34);
            b->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            b->setToolTip(QString("Open %1").arg(t));
            b->setAccessibleName(t + " tab");
            group->addButton(b);
            navButtons.append(b);
            navigation.append(b);
            sideLayout->addWidget(b);
            connect(b, &QPushButton::clicked, this, [this, t]() { navigate(t); });
        }
        sideLayout->addStretch();

        if (navTitles.contains("Settings")) {
            auto *settingsButton = Theme::button("Settings", "side");
            settingsButton->setMinimumHeight(34);
            settingsButton->setToolTip("Open Settings");
            settingsButton->setAccessibleName("Settings tab");
            group->addButton(settingsButton);
            navButtons.append(settingsButton);
            navigation.append(settingsButton);
            sideLayout->addWidget(settingsButton);
            connect(settingsButton, &QPushButton::clicked, this,
                    [this]() { navigate("Settings"); });
        }

        // navButtons must line up with navTitles for syncNavigationSelection(),
        // so reorder the title list to match the sidebar order.
        if (navTitles.contains("Settings")) {
            navTitles.removeAll("Settings");
            navTitles.append("Settings");
        }

        auto *helpRow = Theme::button("Shortcuts  F1", "link");
        helpRow->setToolTip("Show the keyboard shortcuts");
        connect(helpRow, &QPushButton::clicked, this, [this]() { showShortcutHelp(); });
        sideLayout->addWidget(helpRow);

        // ---- top bar
        auto *header = new QFrame;
        header->setObjectName("topbar");
        header->setAttribute(Qt::WA_StyledBackground, true);
        header->setFixedHeight(58);
        topBarLayout = new QHBoxLayout(header);
        auto *topBar = topBarLayout;
        topBar->setContentsMargins(20, 0, 16, 0);
        topBar->setSpacing(10);

        pageTitleLabel = new QLabel(navTitles.value(0));
        pageTitleLabel->setObjectName("pageTitle");
        topBar->addWidget(pageTitleLabel);
        topBar->addStretch();

        auto *searchButton = Theme::button("Search  Ctrl+K", "soft");
        searchButton->setToolTip("Search students, drives, applications and events");
        searchButton->setAccessibleName("Search");
        connect(searchButton, &QPushButton::clicked, this, [this]() { globalSearchDialog(); });
        searchButtonWidget = searchButton;
        topBar->addWidget(searchButton);

        unreadNotifications = store.unreadNotificationCount(session->getEmail());
        notificationButton = Theme::button(
            QString::fromUtf8("🔔 %1").arg(unreadNotifications), "soft");
        notificationButton->setToolTip("Notifications (number of unread messages)");
        notificationButton->setAccessibleName("Notifications");
        connect(notificationButton, &QPushButton::clicked, this, [this]() {
            NotificationsDialog dialog(&store, session->getEmail(), this);
            dialog.exec();
            refreshNotificationCount();
            built.fill(false);
            if (pages) refreshPage(pages->currentIndex());
        });
        topBar->addWidget(notificationButton);

        // One-click light/dark without a trip through Settings.
        themeButton = Theme::button(Theme::isDark() ? QString::fromUtf8("☀")
                                                    : QString::fromUtf8("☾"), "soft");
        themeButton->setToolTip("Switch between light and dark theme (Ctrl+Shift+D)");
        themeButton->setAccessibleName("Toggle dark mode");
        themeButton->setFixedWidth(38);
        connect(themeButton, &QPushButton::clicked, this, [this]() { toggleTheme(); });
        topBar->addWidget(themeButton);

        rolePillLabel = new QLabel(session->role());
        rolePillLabel->setObjectName("rolePill");
        topBar->addWidget(rolePillLabel);
        userNameLabel = new QLabel(displayName);
        userNameLabel->setObjectName("userName");
        userNameLabel->setMaximumWidth(150);
        userNameLabel->setToolTip(displayName);
        topBar->addWidget(userNameLabel);

        auto *accountMenu = new QMenu(header);
        QAction *changePassword = accountMenu->addAction("Change password");
        QAction *shortcuts = accountMenu->addAction("Keyboard shortcuts");
        accountMenu->addSeparator();
        QAction *signOut = accountMenu->addAction("Sign out");
        avatarLabel = new QLabel(Util::initialsOf(displayName));
        avatarLabel->setObjectName("avatar");
        avatarLabel->setFixedSize(30, 30);
        avatarLabel->setAlignment(Qt::AlignCenter);
        topBar->addWidget(avatarLabel);
        accountMenuButton = Theme::button(QString::fromUtf8("▾"), "soft");
        accountMenuButton->setMenu(accountMenu);
        accountMenuButton->setFixedWidth(32);
        accountMenuButton->setToolTip("Change password or sign out");
        accountMenuButton->setAccessibleName("Account menu");
        topBar->addWidget(accountMenuButton);
        connect(changePassword, &QAction::triggered, this, [this]() {
            ChangePasswordDialog dialog(&store, session->getEmail(), this);
            dialog.exec();
        });
        connect(shortcuts, &QAction::triggered, this, [this]() { showShortcutHelp(); });
        connect(signOut, &QAction::triggered, this, [this]() { logout(); });

        const bool navigationVisible = QSettings().value(
            QString("navigation/%1/visible").arg(session->role()), true).toBool();
        sidebar->setVisible(navigationVisible);

        auto *globalSearch = new QShortcut(QKeySequence("Ctrl+K"), w);
        connect(globalSearch, &QShortcut::activated, this, [this]() { globalSearchDialog(); });
        auto *themeShortcut = new QShortcut(QKeySequence("Ctrl+Shift+D"), w);
        connect(themeShortcut, &QShortcut::activated, this, [this]() { toggleTheme(); });
        auto *helpShortcut = new QShortcut(QKeySequence(Qt::Key_F1), w);
        connect(helpShortcut, &QShortcut::activated, this, [this]() { showShortcutHelp(); });
        // Alt+1..9 jumps straight to a tab.
        for (int i = 0; i < navTitles.size() && i < 9; ++i) {
            auto *jump = new QShortcut(QKeySequence(QString("Alt+%1").arg(i + 1)), w);
            const QString target = navTitles[i];
            connect(jump, &QShortcut::activated, this, [this, target]() { navigate(target); });
        }

        pages = new QStackedWidget;
        connect(pages, &QStackedWidget::currentChanged, this, [this](int index) {
            syncNavigationSelection(index);
        });

        auto *mainColumn = new QWidget;
        auto *mainLayout = new QVBoxLayout(mainColumn);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);
        mainLayout->addWidget(header);
        mainLayout->addWidget(pages, 1);

        row->addWidget(sidebar);
        row->addWidget(mainColumn, 1);
        v->addLayout(row);
        updateResponsiveShell();
        return w;
    }

    QWidget *makePage(const QString &title, const Ctx &ctx) {
        const QString role = ctx.user ? ctx.user->role() : QString();
        if ((title == "Dashboard" || title == "Students") &&
            Permissions::isCoordinator(role)) return title == "Students"
                ? static_cast<QWidget *>(new StudentsPage(ctx))
                : static_cast<QWidget *>(new DashboardPage(ctx));
        if ((title == "Dashboard" || title == "Students" || title == "Companies" ||
             title == "Applications" || title == "Reports" || title == "Activity") &&
            !Permissions::isAdmin(role))
            return new QLabel("Access denied.");
        if (title == "Resume Studio")
            return dynamic_cast<Student *>(ctx.user) ? static_cast<QWidget *>(new ResumeStudioPage(ctx))
                                                     : static_cast<QWidget *>(new QLabel("Access denied."));
        if (title == "Dashboard")    return new DashboardPage(ctx);
        if (title == "Drives")       return new DrivesPage(ctx);
        if (title == "Calendar")     return new CalendarPage(ctx);
        if (title == "Students")     return new StudentsPage(ctx);
        if (title == "Companies")    return new CompaniesPage(ctx);
        if (title == "Reports")      return new ReportsPage(ctx);
        if (title == "Applications") return new ApplicationsPage(ctx);
        if (title == "Activity")     return new ActivityPage(ctx);
        if (title == "Settings")     return new SettingsPage(ctx);
        if (title == "My Profile")   return new StudentPortalPage(ctx);
        return new RecruiterPage(ctx);   // "Hiring Suite"
    }

    // Builds (or rebuilds) the page of one tab and keeps the scroll position.
    void refreshPage(int i) {
        if (!pages || !session || i < 0 || i >= navTitles.size()) return;
        const int current = pages->currentIndex();
        QWidget *old = pages->widget(i);
        int scroll = 0;
        if (auto *sa = qobject_cast<QScrollArea *>(old)) scroll = sa->verticalScrollBar()->value();

        const Ctx ctx{ &store, session.get(), [this](const QString &t) { navigate(t); },
                       [this](bool visible) {
                           if (!session) return;
                           QSettings().setValue(QString("navigation/%1/visible").arg(session->role()), visible);
                           if (sidebarFrame) sidebarFrame->setVisible(visible);
                       } };
        QWidget *fresh = makePage(navTitles[i], ctx);
        pages->removeWidget(old);
        old->deleteLater();
        pages->insertWidget(i, fresh);
        pages->setCurrentIndex(current < 0 ? i : current);
        built[i] = true;

        if (scroll > 0)
            QTimer::singleShot(0, fresh, [fresh, scroll]() {
                if (auto *sa = qobject_cast<QScrollArea *>(fresh)) sa->verticalScrollBar()->setValue(scroll);
            });
    }

    void navigate(const QString &title) {
        const int i = navTitles.indexOf(title);
        if (i < 0 || !pages) return;
        if (!built.value(i)) {
            pages->setCurrentIndex(i);
            refreshPage(i);
        }
        pages->setCurrentIndex(i);
        syncNavigationSelection(i);
        if (session) QSettings().setValue(QString("navigation/%1/lastTab").arg(session->role()), title);
    }

    void syncNavigationSelection(int index) {
        if (index < 0 || index >= navButtons.size()) return;
        navButtons[index]->setChecked(true);
        if (pageTitleLabel && index < navTitles.size()) pageTitleLabel->setText(navTitles[index]);
    }

    // Re-reads the unread count from the store; only called when the data changed.
    void refreshNotificationCount() {
        if (!session) return;
        unreadNotifications = store.unreadNotificationCount(session->getEmail());
        updateNotificationLabel();
    }

    // Pure relabelling from the cached count, safe to call from resizeEvent.
    void updateNotificationLabel() {
        if (!notificationButton || !session) return;
        notificationButton->setText(width() < 520
            ? QString::fromUtf8("🔔")
            : QString::fromUtf8("🔔 %1").arg(unreadNotifications));
        notificationButton->setProperty("hasUnread", unreadNotifications > 0);
        notificationButton->style()->unpolish(notificationButton);
        notificationButton->style()->polish(notificationButton);
        notificationButton->setToolTip(unreadNotifications == 0
            ? QStringLiteral("Notifications (none unread)")
            : QString("Notifications (%1 unread)").arg(unreadNotifications));
    }

    void updateResponsiveShell() {
        if (!topBarLayout) return;
        const bool compact = width() < 900;
        const bool veryCompact = width() < 560;
        if (topBarLayout) {
            topBarLayout->setContentsMargins(compact ? 12 : 20, 0, compact ? 10 : 16, 0);
            topBarLayout->setSpacing(compact ? 6 : 10);
        }
        // The sidebar is the first thing to go when there is no width for it; the
        // Alt+1..9 shortcuts and Ctrl+K still reach every page.
        if (sidebarFrame && session) {
            const bool wanted = QSettings().value(
                QString("navigation/%1/visible").arg(session->role()), true).toBool();
            sidebarFrame->setVisible(wanted && !veryCompact);
        }
        if (searchButtonWidget)
            searchButtonWidget->setText(compact ? QStringLiteral("Search") : "Search  Ctrl+K");
        if (rolePillLabel) rolePillLabel->setVisible(!compact);
        if (userNameLabel) userNameLabel->setVisible(!compact);
        if (avatarLabel) avatarLabel->setVisible(!veryCompact);
        if (themeButton) themeButton->setVisible(!veryCompact);
        updateNotificationLabel();
    }

    // Flips the stored theme mode and re-applies the style sheet live. Pages are
    // rebuilt so widgets that paint themselves (cards, rings, charts) repaint
    // against the new palette.
    void toggleTheme() {
        const bool dark = !Theme::isDark();
        QSettings().setValue("theme/mode", dark ? "Dark" : "Light");
        Theme::refresh(*qApp);
        if (themeButton)
            themeButton->setText(dark ? QString::fromUtf8("☀") : QString::fromUtf8("☾"));
        if (pages) {
            built.fill(false);
            refreshPage(pages->currentIndex());
        }
        Theme::toast(this, dark ? "Dark theme enabled" : "Light theme enabled", "info");
    }

    void showShortcutHelp() {
        QStringList rows{
            "<tr><td><b>Ctrl+K</b></td><td>Search students, drives, applications and events</td></tr>",
            "<tr><td><b>Ctrl+Shift+D</b></td><td>Switch between light and dark theme</td></tr>",
            "<tr><td><b>Ctrl+N</b></td><td>Add a record on pages that support it</td></tr>",
            "<tr><td><b>Ctrl+F</b></td><td>Focus the search box on the current page</td></tr>",
            "<tr><td><b>F1</b></td><td>Show this list</td></tr>"
        };
        for (int i = 0; i < navTitles.size() && i < 9; ++i)
            rows << QString("<tr><td><b>Alt+%1</b></td><td>Open %2</td></tr>")
                        .arg(i + 1).arg(navTitles[i].toHtmlEscaped());
        QMessageBox box(this);
        box.setWindowTitle("Keyboard shortcuts");
        box.setTextFormat(Qt::RichText);
        box.setText("<b>Keyboard shortcuts</b><br><table cellpadding='4'>" + rows.join(QString()) +
                    "</table>");
        box.exec();
    }

    void globalSearchDialog() {
        if (!session) return;
        SearchDialog dialog(&store, this);
        if (dialog.exec() != QDialog::Accepted) return;
        navigate(tabForSearchHit(dialog.selectedTab()));
    }

    // Recruiters and students have different tab names; map a hit to a tab they actually have.
    QString tabForSearchHit(const QString &tab) const {
        if (navTitles.contains(tab)) return tab;
        if (tab == "Students" || tab == "Applications") {
            if (navTitles.contains("Hiring Suite")) return "Hiring Suite";
            if (navTitles.contains("My Profile")) return "My Profile";
        }
        return {};
    }

    // The documented demo passwords are public, so nudge the user to replace them.
    void offerPasswordChange() {
        if (!session) return;
        const QString key = QString("security/dismissedDemoPrompt/%1").arg(session->getEmail().toLower());
        if (QSettings().value(key, false).toBool() || !store.usesDefaultPassword(session->getEmail())) return;
        QMessageBox box(this);
        box.setWindowTitle("Secure your account");
        box.setIcon(QMessageBox::Warning);
        box.setText("You are still using a demo password that is listed in the documentation.");
        box.setInformativeText("Change it now to protect this account.");
        QPushButton *change = box.addButton("Change now", QMessageBox::AcceptRole);
        box.addButton("Later", QMessageBox::RejectRole);
        QPushButton *never = box.addButton("Don't ask again", QMessageBox::DestructiveRole);
        box.setDefaultButton(change);
        box.exec();
        if (box.clickedButton() == never) QSettings().setValue(key, true);
        if (box.clickedButton() != change) return;
        ChangePasswordDialog dialog(&store, session->getEmail(), this);
        dialog.exec();
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QMainWindow::resizeEvent(event);
        updateResponsiveShell();
    }

    void closeEvent(QCloseEvent *event) override {
        if (session) {
            const QString key = "window/" + session->role() + "/";
            QSettings settings;
            settings.setValue(key + "size", size());
            settings.setValue(key + "position", pos());
        }
        QMainWindow::closeEvent(event);
    }

    void logout() {
        store.setActor({});
        root->setCurrentWidget(login);
        login->reset();
        if (shell) { root->removeWidget(shell); shell->deleteLater(); shell = nullptr; }
        pages = nullptr;
        notificationButton = nullptr;
        rolePillLabel = nullptr;
        userNameLabel = nullptr;
        brandSubLabel = nullptr;
        avatarLabel = nullptr;
        accountMenuButton = nullptr;
        themeButton = nullptr;
        searchButtonWidget = nullptr;
        pageTitleLabel = nullptr;
        sidebarFrame = nullptr;
        navigationScrollArea = nullptr;
        topBarLayout = nullptr;
        headerLayout = nullptr;
        navButtons.clear();
        built.clear();
        session.reset();
    }
};

#endif // MAINWINDOW_H
