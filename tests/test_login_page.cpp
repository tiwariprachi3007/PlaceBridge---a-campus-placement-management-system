#include "login_page.h"

#include <QtTest>

class LoginPageTests : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        Theme::install(*qApp);
    }

    void welcomeAndMobileLayout() {
        DataStore store(":memory:");
        LoginPage page(&store);
        page.resize(320, 720);
        page.show();
        QCoreApplication::processEvents();

        auto *stack = page.findChild<QStackedWidget *>("welcomeStack");
        QVERIFY(stack);
        QCOMPARE(page.width(), 320);
        QCOMPARE(stack->currentWidget()->objectName(), QString("landingScroll"));
        auto *nav = page.findChild<QWidget *>("landingNav");
        QVERIFY(nav);
        QVERIFY(!nav->isVisible());
        QCOMPARE(page.findChild<QPushButton *>("landingLogin")->text(), QString("Login"));
        auto *loginButton = page.findChild<QPushButton *>("landingLogin");
        auto *getStartedButton = page.findChild<QPushButton *>("landingGetStarted");
        QVERIFY(loginButton->width() >= loginButton->sizeHint().width());
        QVERIFY(getStartedButton->width() >= getStartedButton->sizeHint().width());
        QCOMPARE(page.findChild<QScrollArea *>("landingScroll")->horizontalScrollBarPolicy(),
                 Qt::ScrollBarAlwaysOff);
        auto *landingContent = page.findChild<QWidget *>("landingPage");
        QVERIFY(landingContent);
        for (QWidget *child : landingContent->findChildren<QWidget *>(
                 QString(), Qt::FindDirectChildrenOnly)) {
            if (child->isHidden()) continue;
            const QRect bounds = child->geometry();
            QVERIFY2(bounds.left() >= 0 && bounds.right() < landingContent->width(),
                     qPrintable(child->objectName()));
        }

        auto *studentMetric = page.findChild<QFrame *>("landingMetric_students");
        QVERIFY(studentMetric);
        QCOMPARE(studentMetric->findChild<QLabel *>("landingMetricValue")->text(),
                 QString::number(store.students().size()));
        auto *opportunityMetric = page.findChild<QFrame *>("landingMetric_placement_opportunities");
        QVERIFY(opportunityMetric);
        int openDrives = 0;
        for (const Company &drive : store.drives())
            if (drive.getDriveStatus().compare("Open", Qt::CaseInsensitive) == 0) ++openDrives;
        QCOMPARE(opportunityMetric->findChild<QLabel *>("landingMetricValue")->text(),
                 QString::number(openDrives));

        page.resize(1280, 800);
        QCoreApplication::processEvents();
        QCOMPARE(page.width(), 1280);
        QVERIFY(nav->isVisible());
        page.resize(320, 720);
        QCoreApplication::processEvents();
        QTest::mouseClick(page.findChild<QPushButton *>("landingLogin"), Qt::LeftButton);
        QCOMPARE(stack->currentWidget()->objectName(), QString("authView"));
        QVERIFY(page.findChild<QWidget *>("compactBrandBar")->isVisible());
        QVERIFY(!page.findChild<QWidget *>("showcase")->isVisible());
        QCOMPARE(page.findChild<QScrollArea *>("authFormScroll")->horizontalScrollBarPolicy(),
                 Qt::ScrollBarAlwaysOff);
        QTest::mouseClick(page.findChild<QPushButton *>("authBackLink"), Qt::LeftButton);
        QCOMPARE(stack->currentWidget()->objectName(), QString("landingScroll"));
    }

    void roleSpecificAuthenticationIsPreserved() {
        DataStore store(":memory:");
        LoginPage page(&store);
        page.resize(1200, 800);
        page.show();
        QCoreApplication::processEvents();
        QTest::mouseClick(page.findChild<QPushButton *>("landingLogin"), Qt::LeftButton);

        auto *email = page.findChild<QLineEdit *>("loginEmail");
        auto *password = page.findChild<QLineEdit *>("loginPassword");
        auto *submit = page.findChild<QPushButton *>("loginSubmit");
        QVERIFY(email);
        QVERIFY(password);
        QVERIFY(submit);
        QSignalSpy authenticated(&page, &LoginPage::authenticated);
        QVERIFY(authenticated.isValid());

        QTest::mouseClick(page.findChild<QPushButton *>("role_recruiter"), Qt::LeftButton);
        email->setText("rahul.sharma@placebridge.edu");
        password->setText("student123");
        QTest::mouseClick(submit, Qt::LeftButton);
        QCOMPARE(authenticated.count(), 0);
        QVERIFY(page.findChild<QLabel *>("loginError")->text().contains("registered as Student"));

        email->setText("recruiter@accenture.com");
        password->setText("recruit123");
        QTest::mouseClick(submit, Qt::LeftButton);
        QCOMPARE(authenticated.count(), 1);
        QCOMPARE(authenticated.takeFirst().at(0).toString(), QString("recruiter@accenture.com"));
    }
};

QTEST_MAIN(LoginPageTests)
#include "test_login_page.moc"
