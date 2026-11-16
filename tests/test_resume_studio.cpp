#include "resume_studio.h"

#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

class ResumeStudioTests : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        Theme::install(*qApp);
    }

    void repositoryPersistsAndIsolatesResumes() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        ResumeRepository repository(directory.path());

        ResumeData original;
        original.fullName = "Jordan Student";
        original.email = "jordan@example.edu";
        original.templateName = "Classic";
        original.technicalSkills = "C++, Qt, SQLite";
        ResumeItem project;
        project.title = "Campus Portal";
        project.description = "Built a responsive desktop application.";
        project.technologies = "C++, Qt";
        project.link = "https://github.com/jordan/portal";
        original.projects.append(project);
        ResumeItem education;
        education.title = "Example University";
        education.subtitle = "B.Tech";
        education.detail = "Computer Science";
        education.year = "2027";
        education.score = "8.7";
        original.education.append(education);

        QCOMPARE(repository.save(original.email, original), QString());
        ResumeData restored;
        bool found = false;
        QCOMPARE(repository.load(original.email, &restored, &found), QString());
        QVERIFY(found);
        QCOMPARE(restored.fullName, original.fullName);
        QCOMPARE(restored.templateName, QString("Classic"));
        QCOMPARE(restored.projects.size(), 1);
        QCOMPARE(restored.projects.first().description, project.description);
        QCOMPARE(restored.education.first().year, QString("2027"));
        QVERIFY(!repository.pathFor(original.email).contains(original.email));

        ResumeData other;
        bool otherFound = true;
        QCOMPARE(repository.load("another@example.edu", &other, &otherFound), QString());
        QVERIFY(!otherFound);
        QVERIFY(other.fullName.isEmpty());
    }

    void listEditorAddsEditsAndRemovesEntries() {
        ResumeListEditor editor(ResumeRecordType::Project, "testProject", "Projects",
                                "Add project details.");
        editor.resize(520, 360);
        editor.show();
        QCoreApplication::processEvents();

        auto completeDialog = [](const QString &title, const QString &description) {
            auto *dialog = dynamic_cast<ResumeRecordDialog *>(QApplication::activeModalWidget());
            if (!dialog) return false;
            auto *titleEdit = dialog->findChild<QLineEdit *>("recordTitle");
            auto *descriptionEdit = dialog->findChild<QTextEdit *>("recordDescription");
            auto *save = dialog->findChild<QPushButton *>("resumeRecordSave");
            if (!titleEdit || !descriptionEdit || !save) return false;
            titleEdit->setText(title);
            descriptionEdit->setPlainText(description);
            QTest::mouseClick(save, Qt::LeftButton);
            return true;
        };

        bool added = false;
        QTimer::singleShot(0, [&]() { added = completeDialog("Campus Portal", "First version."); });
        QTest::mouseClick(editor.findChild<QPushButton *>("testProjectAdd"), Qt::LeftButton);
        QVERIFY(added);
        QCOMPARE(editor.items().size(), 1);
        QCOMPARE(editor.items().first().title, QString("Campus Portal"));

        bool edited = false;
        QTimer::singleShot(0, [&]() { edited = completeDialog("Campus Portal v2", "Updated version."); });
        QTest::mouseClick(editor.findChild<QPushButton *>("testProjectEdit"), Qt::LeftButton);
        QVERIFY(edited);
        QCOMPARE(editor.items().first().title, QString("Campus Portal v2"));
        QCOMPARE(editor.items().first().description, QString("Updated version."));

        QTest::mouseClick(editor.findChild<QPushButton *>("testProjectRemove"), Qt::LeftButton);
        QVERIFY(editor.items().isEmpty());
    }

    void previewTemplatesAndPdfExport() {
        ResumeData resume;
        resume.fullName = "Jordan Student";
        resume.email = "jordan@example.edu";
        resume.objective = "Software developer & problem solver.";
        resume.technicalSkills = "C++, Qt, SQLite";
        resume.projects.append({ "Portal <One>", {}, {}, {}, {}, "Built <safely>.", "Qt", "https://example.edu/project" });
        resume.templateName = "Modern";
        const QString modern = ResumeDocumentRenderer::html(resume);
        QVERIFY(modern.contains("&lt;One&gt;"));
        QVERIFY(modern.contains("border-bottom:2px solid #e58a45"));
        QVERIFY(modern.contains("bgcolor=\"#103c35\""));
        QVERIFY(modern.contains("align=\"center\""));
        QVERIFY(modern.contains("font-size:30pt"));
        QVERIFY(!modern.contains("<script>"));
        QTextDocument modernDocument;
        modernDocument.setHtml(modern);
        const QTextCursor nameCursor = modernDocument.find("Jordan Student");
        QVERIFY(!nameCursor.isNull());
        QCOMPARE(nameCursor.charFormat().foreground().color(), QColor("#ffffff"));

        resume.templateName = "Classic";
        const QString classic = ResumeDocumentRenderer::html(resume);
        QVERIFY(classic.contains("font-family:Georgia,serif"));
        QVERIFY(classic != modern);
        resume.templateName = "Minimal";
        const QString minimal = ResumeDocumentRenderer::html(resume);
        QVERIFY(minimal.contains("font-size:21pt"));
        QVERIFY(minimal.contains("font-size:10pt"));
        QVERIFY(!minimal.contains("bgcolor=\"#103c35\""));
        QVERIFY(minimal != classic);
        QVERIFY(minimal != modern);

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath("resume.pdf");
        QCOMPARE(ResumePdfExporter::exportFile(path, resume), QString());
        QFile pdf(path);
        QVERIFY(pdf.open(QIODevice::ReadOnly));
        QCOMPARE(pdf.read(5), QByteArray("%PDF-"));
        QVERIFY(pdf.size() > 1000);
    }

    void pageUpdatesPreviewSavesAndReloads() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        DataStore store(":memory:");
        Student student("Jordan Student", "jordan@example.edu", "B.Tech Computer Science");
        Ctx context{ &store, &student, {}, {} };

        ResumeStudioPage page(context, directory.path());
        page.resize(900, 760);
        page.show();
        QCoreApplication::processEvents();
        auto *name = page.findChild<QLineEdit *>("resumeName");
        auto *objective = page.findChild<QTextEdit *>("resumeObjective");
        auto *templateCombo = page.findChild<QComboBox *>("resumeTemplate");
        auto *preview = page.findChild<QTextBrowser *>("resumePreview");
        auto *email = page.findChild<QLineEdit *>("resumeEmail");
        auto *phone = page.findChild<QLineEdit *>("resumePhone");
        QVERIFY(name && objective && templateCombo && preview && email && phone);
        QCOMPARE(name->height(), email->height());
        QCOMPARE(name->height(), phone->height());
        auto *sectionNav = page.findChild<QListWidget *>("resumeSectionNav");
        QVERIFY(sectionNav);

        name->setText("Jordan A. Student");
        objective->setPlainText("Seeking a software engineering internship.");
        templateCombo->setCurrentText("Minimal");
        QTextCursor minimalName = preview->document()->find("Jordan A. Student");
        QVERIFY(!minimalName.isNull());
        QVERIFY(qAbs(minimalName.charFormat().fontPointSize() - 21.0) < 0.1);
        templateCombo->setCurrentText("Modern");
        QTextCursor modernName = preview->document()->find("Jordan A. Student");
        QVERIFY(!modernName.isNull());
        QVERIFY(qAbs(modernName.charFormat().fontPointSize() - 30.0) < 0.1);
        templateCombo->setCurrentText("Minimal");

        const auto projectSections = sectionNav->findItems("Projects", Qt::MatchExactly);
        QVERIFY(!projectSections.isEmpty());
        sectionNav->setCurrentItem(projectSections.first());
        bool added = false;
        QTimer::singleShot(0, [&]() {
            auto *dialog = dynamic_cast<ResumeRecordDialog *>(QApplication::activeModalWidget());
            if (!dialog) return;
            dialog->findChild<QLineEdit *>("recordTitle")->setText("Study Planner");
            dialog->findChild<QLineEdit *>("recordTechnologies")->setText("C++, Qt");
            dialog->findChild<QTextEdit *>("recordDescription")->setPlainText("Organizes study tasks.");
            QTest::mouseClick(dialog->findChild<QPushButton *>("resumeRecordSave"), Qt::LeftButton);
            added = true;
        });
        QTest::mouseClick(page.findChild<QPushButton *>("projectAdd"), Qt::LeftButton);
        QVERIFY(added);

        QTest::mouseClick(page.findChild<QPushButton *>("saveResume"), Qt::LeftButton);
        QCOMPARE(page.findChild<QLabel *>("resumeStatus")->text(),
                 QString("Resume saved on this device."));

        ResumeStudioPage reopened(context, directory.path());
        reopened.resize(900, 760);
        reopened.show();
        QCoreApplication::processEvents();
        QCOMPARE(reopened.findChild<QLineEdit *>("resumeName")->text(),
                 QString("Jordan A. Student"));
        QCOMPARE(reopened.findChild<QComboBox *>("resumeTemplate")->currentText(),
                 QString("Minimal"));
        QCOMPARE(reopened.findChild<QListWidget *>("projectList")->count(), 1);
        QVERIFY(reopened.findChild<QTextBrowser *>("resumePreview")->document()
                    ->toPlainText().contains("Study Planner"));
    }

    void pageAdaptsThreePanelWorkspaceToWindowWidth() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        DataStore store(":memory:");
        Student student("Jordan Student", "jordan@example.edu", "B.Tech Computer Science");
        Ctx context{ &store, &student, {}, {} };
        ResumeStudioPage page(context, directory.path());
        auto *splitter = page.findChild<QSplitter *>("resumeWorkspaceSplitter");
        auto *navigation = page.findChild<QWidget *>("resumeNavigationPanel");
        auto *sectionNav = page.findChild<QListWidget *>("resumeSectionNav");
        auto *sectionSelect = page.findChild<QComboBox *>("resumeSectionSelect");
        auto *compactNavigation = page.findChild<QWidget *>("resumeCompactNavigation");
        auto *sectionStack = page.findChild<QStackedWidget *>("resumeEditorStack");
        QVERIFY(splitter && navigation && sectionNav && sectionSelect &&
                compactNavigation && sectionStack);
        QCOMPARE(sectionNav->count(), 8);
        for (int row = 0; row < sectionNav->count(); ++row)
            QCOMPARE(sectionNav->item(row)->sizeHint().height(), 54);

        page.resize(1360, 820);
        page.show();
        QCoreApplication::processEvents();
        QCOMPARE(splitter->orientation(), Qt::Horizontal);
        QVERIFY(!navigation->isHidden());
        QVERIFY(compactNavigation->isHidden());
        const QList<int> wideSizes = splitter->sizes();
        QCOMPARE(wideSizes.size(), 3);
        QVERIFY(wideSizes[0] >= 180);
        QVERIFY(wideSizes[1] >= 360);
        QVERIFY(wideSizes[2] >= 360);

        sectionNav->setCurrentRow(4);
        QCOMPARE(sectionStack->currentIndex(), 4);
        QCOMPARE(page.findChild<QLabel *>("resumeSectionTitle")->text(), QString("Projects"));

        page.resize(1100, 820);
        QCoreApplication::processEvents();
        QCOMPARE(splitter->orientation(), Qt::Horizontal);
        QVERIFY(!navigation->isHidden());
        QVERIFY(compactNavigation->isHidden());

        page.resize(1000, 820);
        QCoreApplication::processEvents();
        QCOMPARE(splitter->orientation(), Qt::Horizontal);
        QVERIFY(navigation->isHidden());
        QVERIFY(!compactNavigation->isHidden());

        page.resize(760, 820);
        QCoreApplication::processEvents();
        QCOMPARE(splitter->orientation(), Qt::Vertical);
        QVERIFY(navigation->isHidden());
        QVERIFY(!compactNavigation->isHidden());
        QCOMPARE(sectionSelect->currentIndex(), 4);
        QVERIFY(splitter->width() <= page.viewport()->width());
    }
};

QTEST_MAIN(ResumeStudioTests)
#include "test_resume_studio.moc"
