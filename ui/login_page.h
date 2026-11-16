#ifndef LOGIN_PAGE_H
#define LOGIN_PAGE_H

#include "theme.h"
#include "context.h"
#include "SecurityManager.h"
#include "dialogs.h"
#include "SystemServices.h"
#include <QJsonDocument>
#include <QHostAddress>
#include <QNetworkReply>
#include <QSet>
#include <QSettings>

class PlacementJourney : public QWidget {
public:
    explicit PlacementJourney(QWidget *parent = nullptr) : QWidget(parent) {
        setFixedHeight(154);
        setAccessibleName("PlaceBridge placement journey");
        setAccessibleDescription("Discover opportunities, apply, and get selected.");
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const QRectF panel = QRectF(rect()).adjusted(1, 1, -1, -1);
        QLinearGradient background(panel.topLeft(), panel.bottomRight());
        background.setColorAt(0, QColor(255, 255, 255, 19));
        background.setColorAt(1, QColor(255, 255, 255, 8));
        painter.setBrush(background);
        painter.setPen(QPen(QColor(255, 255, 255, 34), 1));
        painter.drawRoundedRect(panel, 17, 17);

        painter.setPen(QColor("#E58A45"));
        QFont eyebrow(Theme::sans(), 9, QFont::Bold);
        eyebrow.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
        painter.setFont(eyebrow);
        painter.drawText(QRectF(22, 16, width() - 44, 18), Qt::AlignLeft | Qt::AlignVCenter,
                         "ONE CONNECTED PLACEMENT JOURNEY");

        const qreal left = 68;
        const qreal right = width() - 68;
        const qreal y = 81;
        const qreal step = (right - left) / 2;
        const QPointF first(left, y);
        const QPointF middle(left + step, y);
        const QPointF last(right, y);

        QPainterPath bridge;
        bridge.moveTo(first);
        bridge.cubicTo(QPointF(left + step * 0.34, 36), QPointF(left + step * 0.66, 36), middle);
        bridge.cubicTo(QPointF(left + step * 1.34, 36), QPointF(left + step * 1.66, 36), last);
        painter.setPen(QPen(QColor(0xE5, 0x8A, 0x45, 200), 2.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(bridge);

        const QStringList labels = { "Discover", "Apply", "Get selected" };
        const QPointF points[] = { first, middle, last };
        for (int i = 0; i < 3; ++i) {
            painter.setPen(QPen(QColor(255, 255, 255, 140), 2.5));
            painter.setBrush(QColor("#E58A45"));
            painter.drawEllipse(points[i], 12, 12);
            painter.setPen(QColor("#FFFFFF"));
            QFont number(Theme::sans(), 9, QFont::Bold);
            painter.setFont(number);
            painter.drawText(QRectF(points[i].x() - 9, points[i].y() - 9, 18, 18),
                             Qt::AlignCenter, QString::number(i + 1));

            painter.setPen(QColor("#FFFFFF"));
            QFont label(Theme::sans(), 10, QFont::Bold);
            painter.setFont(label);
            painter.drawText(QRectF(points[i].x() - 65, 111, 130, 23),
                             Qt::AlignHCenter | Qt::AlignTop, labels[i]);
        }
    }
};

class ShowcaseFrame : public QFrame {
public:
    explicit ShowcaseFrame(QWidget *parent = nullptr) : QFrame(parent) {
        setObjectName("showcase");
        setAttribute(Qt::WA_StyledBackground, true);
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r(rect());
        QLinearGradient grad(r.topLeft(), r.bottomRight());
        grad.setColorAt(0.0, QColor("#103C35"));
        grad.setColorAt(0.55, QColor("#0B2928"));
        grad.setColorAt(1.0, QColor("#091321"));
        p.fillRect(rect(), grad);

        p.setPen(QPen(QColor(0xE5, 0x8A, 0x45, 150), 2));
        p.drawLine(width() - 1, 0, width() - 1, height());
    }
};

// Split-screen sign-in: forest "showcase" panel on the left, auth card on the right.
// Passwords are verified against salted hashes in the SQLite-backed DataStore.
class LoginPage : public QWidget {
    Q_OBJECT

    DataStore *store;
    QString role = "Student";
    bool createMode = false;

    QLabel *title, *sub, *hint;
    QLabel *error;
    QWidget *nameBox, *extraBox;
    QLabel *extraLabel;
    QLineEdit *nameEdit, *extraEdit, *emailEdit, *pwEdit;
    QPushButton *submitBtn, *toggleBtn, *showPw, *forgotBtn;
    QCheckBox *remember;
    QTimer *lockoutTimer = nullptr;
    RestClient *restClient = nullptr;
    int lockoutSeconds = 0;
    QString lockedEmail;
    QStackedWidget *viewStack = nullptr;
    QWidget *landingView = nullptr;
    QWidget *landingContent = nullptr;
    QWidget *authView = nullptr;
    QWidget *authRow = nullptr;
    QWidget *compactBrand = nullptr;
    QScrollArea *landingScroll = nullptr;
    QHBoxLayout *landingHeaderLayout = nullptr;
    QHBoxLayout *landingBrandLayout = nullptr;
    QVBoxLayout *landingContentLayout = nullptr;
    QPushButton *landingLoginButton = nullptr;
    QPushButton *landingStartButton = nullptr;
    QPushButton *homeButton = nullptr;
    QWidget *landingNavWidget = nullptr;
    QWidget *landingLogo = nullptr;
    QLabel *landingBrandTagline = nullptr;
    QWidget *statsSection = nullptr;
    QWidget *journeySection = nullptr;
    QWidget *benefitsSection = nullptr;
    QHBoxLayout *rootLayout = nullptr;
    QVBoxLayout *formOuterLayout = nullptr;
    QVBoxLayout *cardLayout = nullptr;
    QWidget *showcasePanel = nullptr;

public:
    explicit LoginPage(DataStore *s, QWidget *parent = nullptr) : QWidget(parent), store(s) {
        setObjectName("canvas");
        setAttribute(Qt::WA_StyledBackground, true);
        auto *pageLayout = new QVBoxLayout(this);
        pageLayout->setContentsMargins(0, 0, 0, 0);
        pageLayout->setSpacing(0);
        pageLayout->setSizeConstraint(QLayout::SetNoConstraint);
        viewStack = new QStackedWidget(this);
        viewStack->setObjectName("welcomeStack");
        viewStack->setMinimumSize(0, 0);
        pageLayout->addWidget(viewStack);

        landingView = buildLanding();
        viewStack->addWidget(landingView);

        authView = new QWidget;
        authView->setObjectName("authView");
        auto *authLayout = new QVBoxLayout(authView);
        authLayout->setContentsMargins(0, 0, 0, 0);
        authLayout->setSpacing(0);
        authLayout->setSizeConstraint(QLayout::SetNoConstraint);
        compactBrand = buildCompactBrand();
        authLayout->addWidget(compactBrand);
        authRow = new QWidget;
        authRow->setMinimumWidth(0);
        rootLayout = new QHBoxLayout(authRow);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(0);
        rootLayout->setSizeConstraint(QLayout::SetNoConstraint);
        showcasePanel = buildShowcase();
        showcasePanel->setMinimumWidth(0);
        showcasePanel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        rootLayout->addWidget(showcasePanel, 5);
        rootLayout->addWidget(buildForm(), 4);
        authLayout->addWidget(authRow, 1);
        viewStack->addWidget(authView);
        viewStack->setCurrentWidget(landingView);

        applyRole("Student");
        applyMode();
        emailEdit->setText(QSettings().value("lastEmail").toString());
        remember->setChecked(!emailEdit->text().isEmpty());
        lockoutTimer = new QTimer(this);
        lockoutTimer->setInterval(1000);
        connect(lockoutTimer, &QTimer::timeout, this, [this]() {
            if (--lockoutSeconds <= 0) {
                lockoutTimer->stop();
                submitBtn->setEnabled(true);
                error->setText("You can try signing in again.");
            } else {
                error->setText(QString("Too many failed attempts. Try again in %1 seconds.")
                                   .arg(lockoutSeconds));
            }
        });
    }

    void reset() {
        if (restClient) {
            restClient->logout();
            restClient->deleteLater();
            restClient = nullptr;
        }
        if (lockoutTimer) lockoutTimer->stop();
        lockoutSeconds = 0;
        if (submitBtn) submitBtn->setEnabled(true);
        pwEdit->clear();
        error->hide();
        createMode = false;
        applyMode();
        if (viewStack && landingView) viewStack->setCurrentWidget(landingView);
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        const bool compact = width() < 900;
        if (showcasePanel) showcasePanel->setVisible(!compact);
        if (compactBrand) compactBrand->setVisible(compact);
        if (rootLayout) {
            rootLayout->setStretch(0, compact ? 0 : 5);
            rootLayout->setStretch(1, compact ? 1 : 4);
        }
        if (formOuterLayout)
            formOuterLayout->setContentsMargins(compact ? 12 : 40, compact ? 12 : 30,
                                                 compact ? 12 : 40, compact ? 12 : 30);
        if (cardLayout)
            cardLayout->setContentsMargins(compact ? 20 : 34, compact ? 20 : 34,
                                           compact ? 20 : 34, compact ? 20 : 34);
        if (landingContent) {
            landingContent->setProperty("compact", width() < 640);
            landingContent->style()->unpolish(landingContent);
            landingContent->style()->polish(landingContent);
        }
        if (landingHeaderLayout)
            landingHeaderLayout->setContentsMargins(width() < 420 ? 12 : 28, 12,
                                                     width() < 420 ? 12 : 28, 12);
        if (landingHeaderLayout) landingHeaderLayout->setSpacing(width() < 420 ? 6 : 12);
        if (landingBrandLayout) landingBrandLayout->setSpacing(width() < 420 ? 6 : 10);
        if (landingContentLayout)
            landingContentLayout->setContentsMargins(width() < 420 ? 16 : 28, 0,
                                                      width() < 420 ? 16 : 28, 32);
        if (landingNavWidget) landingNavWidget->setVisible(width() >= 900);
        if (landingLogo) landingLogo->setFixedSize(width() < 420 ? 32 : 38,
                                                    width() < 420 ? 32 : 38);
        if (landingBrandTagline) landingBrandTagline->setVisible(width() >= 420);
        if (landingLoginButton) landingLoginButton->setText(width() < 420 ? "Login" : "Log in");
    }

signals:
    void authenticated(const QString &email);

private:
    QWidget *buildLanding() {
        landingScroll = new QScrollArea;
        landingScroll->setObjectName("landingScroll");
        landingScroll->setFrameShape(QFrame::NoFrame);
        landingScroll->setWidgetResizable(true);
        landingScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        landingScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

        landingContent = new QWidget;
        landingContent->setObjectName("landingPage");
        landingContent->setAttribute(Qt::WA_StyledBackground, true);
        landingContentLayout = new QVBoxLayout(landingContent);
        auto *contentLayout = landingContentLayout;
        contentLayout->setContentsMargins(28, 0, 28, 32);
        contentLayout->setSpacing(34);

        auto *header = new QWidget;
        header->setObjectName("landingHeader");
        landingHeaderLayout = new QHBoxLayout(header);
        landingHeaderLayout->setContentsMargins(28, 12, 28, 12);
        landingHeaderLayout->setSpacing(12);

        landingBrandLayout = new QHBoxLayout;
        auto *brand = landingBrandLayout;
        brand->setSpacing(10);
        landingLogo = Theme::logo("P", true, 38);
        brand->addWidget(landingLogo);
        auto *brandText = new QVBoxLayout;
        brandText->setSpacing(1);
        auto *brandName = new QLabel("PlaceBridge");
        brandName->setObjectName("landingBrand");
        landingBrandTagline = new QLabel("CAMPUS CAREERS, CONNECTED");
        landingBrandTagline->setObjectName("landingTagline");
        brandText->addWidget(brandName);
        brandText->addWidget(landingBrandTagline);
        brand->addLayout(brandText);
        landingHeaderLayout->addLayout(brand);
        landingHeaderLayout->addStretch(1);

        auto *nav = new QWidget;
        landingNavWidget = nav;
        nav->setObjectName("landingNav");
        auto *navLayout = new QHBoxLayout(nav);
        navLayout->setContentsMargins(0, 0, 0, 0);
        navLayout->setSpacing(4);
        auto makeNavLink = [nav](const QString &text) {
            auto *button = new QPushButton(text, nav);
            button->setObjectName("landingNavLink");
            button->setCursor(Qt::PointingHandCursor);
            button->setMinimumHeight(40);
            return button;
        };
        auto *opportunitiesLink = makeNavLink("Opportunities");
        auto *journeyLink = makeNavLink("How it works");
        auto *teamsLink = makeNavLink("For campus teams");
        navLayout->addWidget(opportunitiesLink);
        navLayout->addWidget(journeyLink);
        navLayout->addWidget(teamsLink);
        landingHeaderLayout->addWidget(nav);
        connect(opportunitiesLink, &QPushButton::clicked, this, [this]() {
            if (landingScroll) landingScroll->ensureWidgetVisible(statsSection, 0, 18);
        });
        connect(journeyLink, &QPushButton::clicked, this, [this]() {
            if (landingScroll) landingScroll->ensureWidgetVisible(journeySection, 0, 18);
        });
        connect(teamsLink, &QPushButton::clicked, this, [this]() {
            if (landingScroll) landingScroll->ensureWidgetVisible(benefitsSection, 0, 18);
        });

        landingLoginButton = new QPushButton("Log in");
        landingLoginButton->setObjectName("landingLogin");
        landingLoginButton->setCursor(Qt::PointingHandCursor);
        landingLoginButton->setMinimumHeight(42);
        landingStartButton = new QPushButton("Get started");
        landingStartButton->setObjectName("landingGetStarted");
        landingStartButton->setCursor(Qt::PointingHandCursor);
        landingStartButton->setMinimumHeight(42);
        landingHeaderLayout->addWidget(landingLoginButton);
        landingHeaderLayout->addWidget(landingStartButton);
        connect(landingLoginButton, &QPushButton::clicked, this, [this]() { openAuthentication(false); });
        connect(landingStartButton, &QPushButton::clicked, this, [this]() { openAuthentication(true); });
        contentLayout->addWidget(header);

        auto *hero = new QWidget;
        hero->setObjectName("landingHero");
        auto *heroLayout = new QVBoxLayout(hero);
        heroLayout->setContentsMargins(0, 20, 0, 4);
        auto *heroColumns = new Theme::ResponsiveGrid(390, 34);
        heroColumns->setObjectName("landingHeroColumns");

        auto *copy = new QWidget;
        auto *copyLayout = new QVBoxLayout(copy);
        copyLayout->setContentsMargins(0, 0, 0, 0);
        copyLayout->setSpacing(16);
        auto *eyebrow = new QLabel("A MORE CONNECTED CAREER JOURNEY");
        eyebrow->setObjectName("landingEyebrow");
        copyLayout->addWidget(eyebrow, 0, Qt::AlignLeft);
        auto *headline = new QLabel("Where Talent Meets\nOpportunity.");
        headline->setObjectName("landingHeroTitle");
        headline->setWordWrap(true);
        copyLayout->addWidget(headline);
        auto *description = new QLabel(
            "A smarter way for students, recruiters and placement teams to connect, discover opportunities and build successful careers.");
        description->setObjectName("landingHeroText");
        description->setWordWrap(true);
        description->setMaximumWidth(590);
        copyLayout->addWidget(description);
        auto *heroActions = new Theme::ResponsiveGrid(160, 10);
        auto *heroLogin = new QPushButton("Login to your portal");
        heroLogin->setObjectName("landingPrimary");
        heroLogin->setMinimumHeight(48);
        auto *heroExplore = new QPushButton("Explore opportunities");
        heroExplore->setObjectName("landingSecondary");
        heroExplore->setMinimumHeight(48);
        heroLogin->setCursor(Qt::PointingHandCursor);
        heroExplore->setCursor(Qt::PointingHandCursor);
        heroActions->addWidget(heroLogin);
        heroActions->addWidget(heroExplore);
        copyLayout->addWidget(heroActions);
        connect(heroLogin, &QPushButton::clicked, this, [this]() { openAuthentication(false); });
        connect(heroExplore, &QPushButton::clicked, this, [this]() {
            if (landingScroll) landingScroll->ensureWidgetVisible(statsSection, 0, 18);
        });
        auto *trustLine = new QLabel("One trusted workspace for the people who shape every placement.");
        trustLine->setObjectName("landingTrustLine");
        trustLine->setWordWrap(true);
        copyLayout->addWidget(trustLine);
        heroColumns->addWidget(copy);

        auto *ecosystem = new QFrame;
        ecosystem->setObjectName("landingEcosystem");
        auto *ecosystemLayout = new QVBoxLayout(ecosystem);
        ecosystemLayout->setContentsMargins(22, 22, 22, 22);
        ecosystemLayout->setSpacing(18);
        auto *ecosystemHeader = new QHBoxLayout;
        auto *ecosystemTitle = new QLabel("One connected ecosystem");
        ecosystemTitle->setObjectName("ecosystemTitle");
        auto *ecosystemStatus = new QLabel("PLACEBRIDGE");
        ecosystemStatus->setObjectName("ecosystemBadge");
        ecosystemHeader->addWidget(ecosystemTitle, 1);
        ecosystemHeader->addWidget(ecosystemStatus);
        ecosystemLayout->addLayout(ecosystemHeader);

        auto *ecosystemNodes = new QHBoxLayout;
        ecosystemNodes->setContentsMargins(0, 4, 0, 4);
        ecosystemNodes->setSpacing(8);
        const QStringList nodeTitles = { "Students", "Recruiters", "Campus teams" };
        const QStringList nodeNumbers = { "01", "02", "03" };
        for (int i = 0; i < nodeTitles.size(); ++i) {
            auto *node = new QFrame;
            node->setObjectName("ecosystemNode");
            auto *nodeLayout = new QVBoxLayout(node);
            nodeLayout->setContentsMargins(10, 12, 10, 12);
            nodeLayout->setSpacing(7);
            auto *number = new QLabel(nodeNumbers[i]);
            number->setObjectName("ecosystemNodeNumber");
            auto *name = new QLabel(nodeTitles[i]);
            name->setObjectName("ecosystemNodeName");
            name->setWordWrap(true);
            nodeLayout->addWidget(number);
            nodeLayout->addWidget(name);
            ecosystemNodes->addWidget(node, 1);
        }
        ecosystemLayout->addLayout(ecosystemNodes);

        auto *ecosystemDivider = new QFrame;
        ecosystemDivider->setObjectName("ecosystemDivider");
        ecosystemLayout->addWidget(ecosystemDivider);
        const QVector<Company> drives = store->drives();
        int openDrives = 0;
        for (const Company &drive : drives)
            if (drive.getDriveStatus().compare("Open", Qt::CaseInsensitive) == 0) ++openDrives;
        auto *ecosystemFoot = new QHBoxLayout;
        auto *liveLabel = new QLabel("OPEN OPPORTUNITIES");
        liveLabel->setObjectName("ecosystemFootLabel");
        auto *liveCount = new QLabel(QString::number(openDrives));
        liveCount->setObjectName("ecosystemFootValue");
        auto *liveDescription = new QLabel("listed in the portal");
        liveDescription->setObjectName("ecosystemFootLabel");
        ecosystemFoot->addWidget(liveLabel);
        ecosystemFoot->addStretch();
        ecosystemFoot->addWidget(liveCount);
        ecosystemFoot->addWidget(liveDescription);
        ecosystemLayout->addLayout(ecosystemFoot);
        heroColumns->addWidget(ecosystem);
        heroLayout->addWidget(heroColumns);
        contentLayout->addWidget(hero);

        statsSection = new QWidget;
        statsSection->setObjectName("landingStats");
        auto *statsLayout = new QVBoxLayout(statsSection);
        statsLayout->setContentsMargins(0, 8, 0, 0);
        statsLayout->setSpacing(16);
        auto *statsHeading = new QLabel("Real opportunities. A clearer picture.");
        statsHeading->setObjectName("landingSectionTitle");
        auto *statsCaption = new QLabel("Live counts from the records in this PlaceBridge workspace.");
        statsCaption->setObjectName("landingSectionText");
        statsLayout->addWidget(statsHeading);
        statsLayout->addWidget(statsCaption);

        QSet<QString> partners;
        for (const Company &drive : drives) partners.insert(drive.getName().trimmed().toCaseFolded());
        auto *metrics = new Theme::ResponsiveGrid(220, 14);
        metrics->addWidget(landingMetric(QString::number(store->students().size()), "Students",
                                         "Student records in the portal"));
        metrics->addWidget(landingMetric(QString::number(partners.size()), "Hiring partners",
                                         "Organizations represented by drives"));
        metrics->addWidget(landingMetric(QString::number(openDrives), "Placement opportunities",
                                         "Currently open placement drives"));
        statsLayout->addWidget(metrics);
        contentLayout->addWidget(statsSection);

        journeySection = new QFrame;
        journeySection->setObjectName("landingJourneyPanel");
        auto *journeyLayout = new QVBoxLayout(journeySection);
        journeyLayout->setContentsMargins(24, 22, 24, 22);
        journeyLayout->setSpacing(14);
        auto *journeyTitle = new QLabel("A journey with a clear next step");
        journeyTitle->setObjectName("landingJourneyTitle");
        auto *journeyText = new QLabel("From discovering the right role to celebrating a successful match.");
        journeyText->setObjectName("landingJourneyText");
        journeyText->setWordWrap(true);
        journeyLayout->addWidget(journeyTitle);
        journeyLayout->addWidget(journeyText);
        journeyLayout->addWidget(new PlacementJourney);
        contentLayout->addWidget(journeySection);

        benefitsSection = new QWidget;
        benefitsSection->setObjectName("landingBenefits");
        auto *benefitsLayout = new QVBoxLayout(benefitsSection);
        benefitsLayout->setContentsMargins(0, 4, 0, 0);
        benefitsLayout->setSpacing(16);
        auto *benefitsTitle = new QLabel("Designed around the people behind every placement");
        benefitsTitle->setObjectName("landingSectionTitle");
        benefitsTitle->setWordWrap(true);
        auto *benefitCards = new Theme::ResponsiveGrid(250, 14);
        benefitCards->addWidget(landingBenefit("For students", "Discover relevant drives, keep applications organized and stay on top of important dates."));
        benefitCards->addWidget(landingBenefit("For recruiters", "Review candidate information and manage hiring activity through a focused workspace."));
        benefitCards->addWidget(landingBenefit("For placement teams", "Coordinate student records, opportunities and outcomes in one connected portal."));
        benefitsLayout->addWidget(benefitsTitle);
        benefitsLayout->addWidget(benefitCards);
        contentLayout->addWidget(benefitsSection);

        auto *footer = new QWidget;
        footer->setObjectName("landingFooter");
        auto *footerLayout = new QHBoxLayout(footer);
        footerLayout->setContentsMargins(0, 16, 0, 0);
        footerLayout->setSpacing(12);
        auto *footerText = new QLabel("PlaceBridge  |  Connecting campus talent with meaningful opportunity.");
        footerText->setObjectName("landingFooterText");
        footerText->setWordWrap(true);
        auto *footerLogin = new QPushButton("Sign in");
        footerLogin->setObjectName("landingNavLink");
        footerLogin->setCursor(Qt::PointingHandCursor);
        footerLayout->addWidget(footerText, 1);
        footerLayout->addWidget(footerLogin);
        connect(footerLogin, &QPushButton::clicked, this, [this]() { openAuthentication(false); });
        contentLayout->addWidget(footer);

        landingScroll->setWidget(landingContent);
        return landingScroll;
    }

    QFrame *landingMetric(const QString &value, const QString &title, const QString &detail) {
        auto *frame = new QFrame;
        // The per-metric object name is what the UI tests look the card up by, so
        // the shared card styling hangs off a property instead of #landingMetric
        // (which no widget ever actually carried, leaving these cards unstyled).
        frame->setObjectName("landingMetric_" + title.toLower().replace(' ', '_'));
        frame->setProperty("landingCard", "metric");
        frame->setAttribute(Qt::WA_StyledBackground, true);
        auto *layout = new QVBoxLayout(frame);
        layout->setContentsMargins(20, 18, 20, 18);
        layout->setSpacing(6);
        auto *number = new QLabel(value);
        number->setObjectName("landingMetricValue");
        auto *name = new QLabel(title);
        name->setObjectName("landingMetricTitle");
        auto *description = new QLabel(detail);
        description->setObjectName("landingMetricText");
        description->setWordWrap(true);
        layout->addWidget(number);
        layout->addWidget(name);
        layout->addWidget(description);
        return frame;
    }

    QFrame *landingBenefit(const QString &title, const QString &description) {
        auto *frame = new QFrame;
        frame->setObjectName("landingBenefitCard");
        auto *layout = new QVBoxLayout(frame);
        layout->setContentsMargins(20, 18, 20, 18);
        layout->setSpacing(8);
        auto *name = new QLabel(title);
        name->setObjectName("landingBenefitTitle");
        auto *text = new QLabel(description);
        text->setObjectName("landingBenefitText");
        text->setWordWrap(true);
        layout->addWidget(name);
        layout->addWidget(text);
        return frame;
    }

    QWidget *buildCompactBrand() {
        auto *bar = new QWidget;
        bar->setObjectName("compactBrandBar");
        auto *layout = new QHBoxLayout(bar);
        layout->setContentsMargins(18, 12, 18, 10);
        layout->setSpacing(10);
        layout->addWidget(Theme::logo("P", true, 34));
        auto *copy = new QVBoxLayout;
        copy->setSpacing(1);
        auto *name = new QLabel("PlaceBridge");
        name->setObjectName("compactBrandName");
        auto *tagline = new QLabel("Where Talent Meets Opportunity");
        tagline->setObjectName("compactBrandTagline");
        copy->addWidget(name);
        copy->addWidget(tagline);
        layout->addLayout(copy, 1);
        return bar;
    }

    void openAuthentication(bool registerAccount) {
        if (!viewStack || !authView) return;
        createMode = registerAccount;
        error->hide();
        applyMode();
        viewStack->setCurrentWidget(authView);
        if (registerAccount) nameEdit->setFocus();
        else emailEdit->setFocus();
    }

    QWidget *buildShowcase() {
        auto *f = new QFrame;
        f->setObjectName("showcase");
        auto *v = new QVBoxLayout(f);
        v->setContentsMargins(52, 38, 52, 38);
        v->setSpacing(15);

        auto *brand = new QHBoxLayout;
        brand->setSpacing(11);
        brand->addWidget(Theme::logo("P", true, 42));
        auto *bn = new QLabel("PlaceBridge");
        bn->setObjectName("showBrand");
        brand->addWidget(bn);
        brand->addStretch();
        v->addLayout(brand);

        auto *pill = new QLabel("CAMPUS PLACEMENTS, CONNECTED");
        pill->setObjectName("showPill");
        v->addWidget(pill, 0, Qt::AlignLeft);
        auto *t = new QLabel("Welcome to");
        t->setObjectName("showTitle");
        v->addWidget(t);
        auto *accentTitle = new QLabel("PlaceBridge.");
        accentTitle->setObjectName("showAccentTitle");
        v->addWidget(accentTitle);
        auto *s = new QLabel("Where Talent Meets Opportunity. Bring students, recruiters and placement teams "
                             "together in one trusted career workspace.");
        s->setObjectName("showSub");
        s->setWordWrap(true);
        s->setMaximumWidth(540);
        v->addWidget(s);

        QSet<QString> cos;
        const auto drives = store->drives();
        int openDrives = 0;
        for (const Company &d : drives) {
            cos.insert(d.getName().trimmed().toCaseFolded());
            if (d.getDriveStatus().compare("Open", Qt::CaseInsensitive) == 0) ++openDrives;
        }
        auto *stats = new QHBoxLayout;
        stats->setSpacing(12);
        auto stat = [&](const QString &val, const QString &lab) {
            auto *tile = new QFrame;
            tile->setObjectName("showMetric");
            auto *b = new QVBoxLayout(tile);
            b->setContentsMargins(14, 11, 14, 11);
            b->setSpacing(2);
            auto *a = new QLabel(val);
            a->setObjectName("showStat");
            auto *l = new QLabel(lab);
            l->setObjectName("showStatLabel");
            l->setWordWrap(true);
            b->addWidget(a);
            b->addWidget(l);
            stats->addWidget(tile, 1);
        };
        stat(QString::number(store->students().size()), "Students");
        stat(QString::number(cos.size()), "Hiring partners");
        stat(QString::number(openDrives), "Open opportunities");
        v->addLayout(stats);
        v->addWidget(new PlacementJourney);
        auto *privacy = new QLabel("A focused, role-based workspace for every step.");
        privacy->setObjectName("showPrivacy");
        v->addWidget(privacy);
        return f;
    }

    QWidget *field(QVBoxLayout *v, const QString &labelText, QLineEdit *edit, QLabel **labelOut = nullptr) {
        auto *box = new QWidget;
        auto *b = new QVBoxLayout(box);
        b->setContentsMargins(0, 0, 0, 0);
        b->setSpacing(5);
        auto *l = Theme::label(labelText.toUpper(), "eyebrow");
        b->addWidget(l);
        b->addWidget(edit);
        v->addWidget(box);
        if (labelOut) *labelOut = l;
        return box;
    }

    QWidget *buildForm() {
        auto *scroll = new QScrollArea;
        scroll->setObjectName("authFormScroll");
        scroll->setMinimumWidth(0);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        auto *side = new QWidget;
        scroll->setWidget(side);
        side->setObjectName("canvas");
        side->setAttribute(Qt::WA_StyledBackground, true);
        formOuterLayout = new QVBoxLayout(side);
        auto *outer = formOuterLayout;
        outer->setContentsMargins(40, 30, 40, 30);
        outer->addStretch();

        auto *c = new Theme::CardFrame(24);
        c->setMinimumWidth(0);
        c->setMaximumWidth(480);
        cardLayout = Theme::vbox(c, 34, 12);
        auto *v = cardLayout;

        homeButton = new QPushButton("< Back to PlaceBridge");
        homeButton->setObjectName("authBackLink");
        homeButton->setCursor(Qt::PointingHandCursor);
        homeButton->setMinimumHeight(36);
        v->addWidget(homeButton, 0, Qt::AlignLeft);
        connect(homeButton, &QPushButton::clicked, this, [this]() {
            if (viewStack && landingView) viewStack->setCurrentWidget(landingView);
        });

        title = Theme::label("", "bigName");
        sub = Theme::label("", "muted", nullptr, true);
        v->addWidget(title);
        v->addWidget(sub);

        // role selector
        auto *roles = new Theme::ResponsiveGrid(170, 8);
        auto *grp = new QButtonGroup(this);
        for (const QString &r : QStringList{ "Student", "Recruiter", "Department Coordinator", "Admin" }) {
            auto *b = Theme::button(r, "role");
            b->setObjectName("role_" + r.toLower().replace(' ', '_'));
            b->setMinimumHeight(44);
            b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            b->setChecked(r == "Student");
            grp->addButton(b);
            roles->addWidget(b);
            connect(b, &QPushButton::clicked, this, [this, r]() { applyRole(r); });
        }
        v->addWidget(roles);

        nameEdit = new QLineEdit; nameEdit->setObjectName("loginName"); nameEdit->setPlaceholderText("Your full name");
        extraEdit = new QLineEdit; extraEdit->setObjectName("loginDetails");
        emailEdit = new QLineEdit; emailEdit->setObjectName("loginEmail"); emailEdit->setPlaceholderText("name@placebridge.edu");
        pwEdit = new QLineEdit; pwEdit->setObjectName("loginPassword"); pwEdit->setEchoMode(QLineEdit::Password); pwEdit->setPlaceholderText("Password");

        nameBox = field(v, "Full name", nameEdit);
        extraBox = field(v, "Details", extraEdit, &extraLabel);
        field(v, "E-mail address", emailEdit);

        auto *pwRow = new QHBoxLayout;
        pwRow->setSpacing(8);
        pwRow->addWidget(pwEdit, 1);
        showPw = Theme::button("Show", "soft");
        showPw->setCheckable(true);
        pwRow->addWidget(showPw);
        auto *pwBox = new QWidget;
        auto *pb = new QVBoxLayout(pwBox);
        pb->setContentsMargins(0, 0, 0, 0);
        pb->setSpacing(5);
        pb->addWidget(Theme::label("PASSWORD", "eyebrow"));
        pb->addLayout(pwRow);
        v->addWidget(pwBox);
        connect(showPw, &QPushButton::toggled, this, [this](bool on) {
            pwEdit->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
            showPw->setText(on ? "Hide" : "Show");
        });

        remember = new QCheckBox("Remember my e-mail on this device");
        v->addWidget(remember);

        error = Theme::label("", "error", nullptr, true);
        error->setObjectName("loginError");
        error->hide();
        v->addWidget(error);

        submitBtn = Theme::button("Sign In", "primary");
        submitBtn->setObjectName("loginSubmit");
        submitBtn->setMinimumHeight(44);
        submitBtn->setDefault(true);
        v->addWidget(submitBtn);

        toggleBtn = Theme::button("", "link");
        v->addWidget(toggleBtn, 0, Qt::AlignHCenter);
        forgotBtn = Theme::button("Forgot password?", "link");
        v->addWidget(forgotBtn, 0, Qt::AlignHCenter);
        connect(forgotBtn, &QPushButton::clicked, this, [this]() {
            RedeemResetCodeDialog dialog(store, emailEdit->text().trimmed(), this);
            if (dialog.exec() != QDialog::Accepted) return;
            pwEdit->clear();
            error->hide();
            Theme::toast(this, "Password updated. Sign in with your new password.");
        });
        hint = Theme::label("", "muted", nullptr, true);
        hint->setAlignment(Qt::AlignCenter);
        v->addWidget(hint);

        connect(submitBtn, &QPushButton::clicked, this, [this]() { submit(); });
        connect(emailEdit, &QLineEdit::returnPressed, this, [this]() { submit(); });
        connect(pwEdit, &QLineEdit::returnPressed, this, [this]() { submit(); });
        connect(toggleBtn, &QPushButton::clicked, this, [this]() {
            createMode = !createMode;
            error->hide();
            applyMode();
        });

        outer->addWidget(c, 0, Qt::AlignHCenter);
        outer->addStretch();
        return scroll;
    }

    void applyRole(const QString &r) {
        role = r;
        if (r == "Student") extraLabel->setText("COURSE / BRANCH");
        else if (r == "Recruiter") extraLabel->setText("COMPANY NAME");
        else if (r == "Department Coordinator") extraLabel->setText("BRANCH");
        else extraLabel->setText("EMPLOYEE ID");
        extraEdit->setPlaceholderText(r == "Student" ? "e.g. B.Tech Computer Science"
            : (r == "Recruiter" ? "e.g. Accenture"
                : (r == "Department Coordinator" ? "e.g. Computer Science" : "e.g. PO-2024")));
        if (r == "Student") hint->setText("Demo student: rahul.sharma@placebridge.edu / student123");
        else if (r == "Recruiter") hint->setText("Demo recruiter: recruiter@accenture.com / recruit123");
        else if (r == "Department Coordinator")
            hint->setText("Coordinator accounts are issued by the placement office.");
        else hint->setText("Demo admin: officer@placebridge.edu / admin123");
        error->hide();
    }

    void applyMode() {
        title->setText(createMode ? "Create your profile" : "Welcome back");
        sub->setText(createMode ? "Register a new PlaceBridge account for the 2026 season."
                                : "Sign in to continue to your PlaceBridge workspace.");
        nameBox->setVisible(createMode);
        extraBox->setVisible(createMode);
        submitBtn->setText(createMode ? tr("Create Account") : tr("Sign In"));
        toggleBtn->setText(createMode ? "Already registered? Sign in" : "New here? Create a profile");
        hint->setVisible(!createMode);
        forgotBtn->setVisible(!createMode);
    }

    void showError(const QString &m) { error->setText(m); error->show(); }

    void submit() {
        const QString email = emailEdit->text().trimmed();
        const QString pw = pwEdit->text();
        const QString accountRole = role;

        const QString emailError = Validator::emailError(email);
        if (!emailError.isEmpty()) return showError(emailError);
        if (createMode) {
            if (role == "Admin" || role == "Department Coordinator")
                return showError("Admin and Department Coordinator accounts cannot be self-registered. Sign in as an Admin and create them in Settings.");
            const QString nameError = Validator::requiredTextError(nameEdit->text(), "your full name");
            if (!nameError.isEmpty()) return showError(nameError);
            const QString passwordError = Validator::passwordError(pw);
            if (!passwordError.isEmpty()) return showError(passwordError);
            if (role != "Admin" && extraEdit->text().trimmed().isEmpty())
                return showError(role == "Student" ? "Please enter your course."
                    : (role == "Recruiter" ? "Please enter your company name." : "Please enter your branch."));
            const QString err = store->registerAccount(email, pw, accountRole, nameEdit->text().trimmed(), extraEdit->text().trimmed());
            if (!err.isEmpty()) return showError(err);
            store->setActor(email);
            if (role == "Student" &&
                !store->addStudent(Student(nameEdit->text().trimmed(), email, extraEdit->text().trimmed()))) {
                store->setActor({});
                return showError("Could not create the student record. Contact the placement office.");
            }
        }

        const int secondsLeft = store->loginLockoutSeconds(email);
        if (secondsLeft > 0) return beginLockout(email, secondsLeft);
        const QSettings settings;
        if (settings.value("network/enabled", false).toBool()) {
            const QUrl endpoint(settings.value("network/endpoint", "http://127.0.0.1:8080").toString());
            QHostAddress serverAddress;
            const bool loopback = endpoint.host() == "localhost" ||
                (serverAddress.setAddress(endpoint.host()) && serverAddress.isLoopback());
            if (!endpoint.isValid() || endpoint.scheme() != "http" || !loopback)
                return showError("The REST server must use a valid localhost URL (HTTP credentials are not sent to remote hosts).");
            submitBtn->setEnabled(false);
            error->setText("Authenticating with the PlaceBridge server…");
            error->show();
            restClient = new RestClient(endpoint, this);
            QNetworkReply *reply = restClient->login(email, pw);
            connect(reply, &QNetworkReply::finished, this, [this, reply, email, accountRole]() {
                const QNetworkReply::NetworkError networkError = reply->error();
                const QByteArray payload = reply->readAll();
                const int lockout = store->loginLockoutSeconds(email);
                reply->deleteLater();
                submitBtn->setEnabled(true);
                if (networkError != QNetworkReply::NoError) {
                    if (lockout > 0) beginLockout(email, lockout);
                    else showError("Server sign-in failed. Check the server address and credentials.");
                    return;
                }
                const QJsonObject response = QJsonDocument::fromJson(payload).object();
                const QString token = response.value("token").toString();
                if (token.isEmpty()) return showError("The server returned an invalid sign-in response.");
                restClient->setToken(token);
                const auto account = store->findAccount(email);
                if (!account) {
                    restClient->logout();
                    return showError("Server sign-in succeeded, but this desktop is not connected to the same PlaceBridge data store.");
                }
                if (account->role != accountRole) {
                    restClient->logout();
                    return showError(QString("This account is registered as %1. Please select that role above.")
                                         .arg(account->role));
                }
                finishSignIn(account->email);
            });
            return;
        }
        const auto acc = store->authenticate(email, pw);
        if (!acc) {
            const int seconds = store->recordFailedLogin(email);
            if (seconds > 0) return beginLockout(email, seconds);
            return showError("Incorrect e-mail or password.");
        }
        if (acc->role != accountRole)
            return showError(QString("This account is registered as %1. Please select that role above.").arg(acc->role));

        finishSignIn(acc->email);
    }

    void finishSignIn(const QString &email) {
        store->setActor(email);
        store->logActivity("login", "Signed in");
        QSettings s;
        if (remember->isChecked()) s.setValue("lastEmail", email);
        else s.remove("lastEmail");
        error->hide();
        emit authenticated(email);
    }

    void beginLockout(const QString &email, int seconds) {
        lockedEmail = email;
        lockoutSeconds = seconds;
        submitBtn->setEnabled(false);
        error->setText(QString("Too many failed attempts. Try again in %1 seconds.").arg(seconds));
        error->show();
        if (!lockoutTimer->isActive()) lockoutTimer->start();
    }
};

#endif // LOGIN_PAGE_H
