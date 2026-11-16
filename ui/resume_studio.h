#ifndef RESUME_STUDIO_H
#define RESUME_STUDIO_H

#include "context.h"
#include "resume_preview.h"
#include "theme.h"
#include "SecurityManager.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QTextBrowser>

enum class ResumeRecordType {
    Education,
    Project,
    Experience,
    Certification,
    Achievement,
    Activity
};

class ResumeRecordDialog : public QDialog {
    ResumeRecordType type;
    ResumeItem value;
    QLineEdit *titleEdit = nullptr;
    QLineEdit *subtitleEdit = nullptr;
    QLineEdit *detailEdit = nullptr;
    QLineEdit *yearEdit = nullptr;
    QLineEdit *scoreEdit = nullptr;
    QLineEdit *technologiesEdit = nullptr;
    QLineEdit *linkEdit = nullptr;
    QTextEdit *descriptionEdit = nullptr;
    QLabel *errorLabel = nullptr;

public:
    ResumeRecordDialog(ResumeRecordType recordType, const ResumeItem &record = {},
                       QWidget *parent = nullptr)
        : QDialog(parent), type(recordType), value(record) {
        setWindowTitle(record.title.isEmpty() ? "Add resume entry" : "Edit resume entry");
        setMinimumSize(320, 360);
        resize(480, 560);
        auto *outer = new QVBoxLayout(this);
        outer->setContentsMargins(12, 12, 12, 12);
        outer->setSpacing(10);

        auto *scroll = new QScrollArea;
        scroll->setObjectName("resumeEntryScroll");
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto *content = new QWidget;
        auto *fields = new QVBoxLayout(content);
        fields->setContentsMargins(8, 8, 8, 8);
        fields->setSpacing(10);

        auto addLine = [fields](const QString &labelText, const QString &placeholder,
                                const QString &objectName) {
            auto *box = new QWidget;
            auto *layout = new QVBoxLayout(box);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(4);
            auto *label = Theme::label(labelText.toUpper(), "eyebrow");
            auto *edit = new QLineEdit;
            edit->setObjectName(objectName);
            edit->setPlaceholderText(placeholder);
            edit->setFixedHeight(42);
            layout->addWidget(label);
            layout->addWidget(edit);
            fields->addWidget(box);
            return edit;
        };
        auto addDescription = [fields]() {
            auto *box = new QWidget;
            auto *layout = new QVBoxLayout(box);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(4);
            auto *label = Theme::label("DESCRIPTION", "eyebrow");
            auto *edit = new QTextEdit;
            edit->setObjectName("recordDescription");
            edit->setMinimumHeight(120);
            edit->setPlaceholderText("Describe the work, outcome or contribution.");
            layout->addWidget(label);
            layout->addWidget(edit);
            fields->addWidget(box);
            return edit;
        };

        if (type == ResumeRecordType::Education) {
            titleEdit = addLine("Institution", "University or college name", "recordTitle");
            subtitleEdit = addLine("Degree", "e.g. B.Tech", "recordSubtitle");
            detailEdit = addLine("Branch / Major", "e.g. Computer Science", "recordDetail");
            yearEdit = addLine("Graduation year", "e.g. 2027", "recordYear");
            scoreEdit = addLine("CGPA", "e.g. 8.6 / 10", "recordScore");
        } else if (type == ResumeRecordType::Project) {
            titleEdit = addLine("Project title", "Name of the project", "recordTitle");
            technologiesEdit = addLine("Technologies", "e.g. C++, Qt, SQLite", "recordTechnologies");
            linkEdit = addLine("GitHub / project link", "https://github.com/...", "recordLink");
            descriptionEdit = addDescription();
        } else if (type == ResumeRecordType::Experience) {
            titleEdit = addLine("Organization", "Company or organization", "recordTitle");
            subtitleEdit = addLine("Role", "Job title or internship role", "recordSubtitle");
            detailEdit = addLine("Dates", "e.g. May 2025 - Aug 2025", "recordDetail");
            descriptionEdit = addDescription();
        } else {
            titleEdit = addLine(type == ResumeRecordType::Certification ? "Certification"
                              : type == ResumeRecordType::Achievement ? "Achievement"
                                                                      : "Activity",
                                "Title", "recordTitle");
            descriptionEdit = addDescription();
        }
        fields->addStretch();
        scroll->setWidget(content);
        outer->addWidget(scroll, 1);

        errorLabel = Theme::label("", "error", nullptr, true);
        errorLabel->setObjectName("recordError");
        errorLabel->hide();
        outer->addWidget(errorLabel);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
        buttons->button(QDialogButtonBox::Save)->setObjectName("resumeRecordSave");
        for (QAbstractButton *button : buttons->buttons())
            button->setMinimumSize(96, 40);
        connect(buttons, &QDialogButtonBox::accepted, this, [this]() { validateAndAccept(); });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        outer->addWidget(buttons);
        populate();
    }

    ResumeItem record() const {
        ResumeItem result;
        result.title = titleEdit ? titleEdit->text().trimmed() : QString();
        result.subtitle = subtitleEdit ? subtitleEdit->text().trimmed() : QString();
        result.detail = detailEdit ? detailEdit->text().trimmed() : QString();
        result.year = yearEdit ? yearEdit->text().trimmed() : QString();
        result.score = scoreEdit ? scoreEdit->text().trimmed() : QString();
        result.technologies = technologiesEdit ? technologiesEdit->text().trimmed() : QString();
        result.link = linkEdit ? linkEdit->text().trimmed() : QString();
        result.description = descriptionEdit ? descriptionEdit->toPlainText().trimmed() : QString();
        return result;
    }

private:
    void populate() {
        if (titleEdit) titleEdit->setText(value.title);
        if (subtitleEdit) subtitleEdit->setText(value.subtitle);
        if (detailEdit) detailEdit->setText(value.detail);
        if (yearEdit) yearEdit->setText(value.year);
        if (scoreEdit) scoreEdit->setText(value.score);
        if (technologiesEdit) technologiesEdit->setText(value.technologies);
        if (linkEdit) linkEdit->setText(value.link);
        if (descriptionEdit) descriptionEdit->setPlainText(value.description);
    }

    void validateAndAccept() {
        const ResumeItem updated = record();
        const bool valid = type == ResumeRecordType::Education
            ? (!updated.title.isEmpty() || !updated.subtitle.isEmpty())
            : type == ResumeRecordType::Experience
                ? (!updated.title.isEmpty() || !updated.subtitle.isEmpty())
                : !updated.title.isEmpty();
        if (!valid) {
            errorLabel->setText(type == ResumeRecordType::Education
                ? "Enter at least an institution or a degree."
                : "Please enter a title for this entry.");
            errorLabel->show();
            return;
        }
        value = updated;
        accept();
    }
};

class ResumeListEditor : public QWidget {
    ResumeRecordType type;
    QString key;
    QListWidget *list = nullptr;
    QStackedWidget *listStack = nullptr;
    QWidget *emptyState = nullptr;
    QPushButton *editButton = nullptr;
    QPushButton *removeButton = nullptr;
    QBoxLayout *actionsLayout = nullptr;
    QVector<ResumeItem> values;
    std::function<void(const QVector<ResumeItem> &)> onChanged;

public:
    ResumeListEditor(ResumeRecordType recordType, const QString &objectKey,
                     const QString &heading, const QString &description,
                     QWidget *parent = nullptr)
        : QWidget(parent), type(recordType), key(objectKey) {
        setObjectName(key + "ListEditor");
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(10);
        if (!heading.isEmpty())
            layout->addWidget(Theme::sectionHead(heading, description));
        listStack = new QStackedWidget;
        listStack->setObjectName(key + "ListStack");
        listStack->setMinimumHeight(150);
        list = new QListWidget;
        list->setObjectName(key + "List");
        list->setAlternatingRowColors(true);
        list->setWordWrap(true);
        list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        emptyState = Theme::emptyState("No entries yet", "Choose Add entry to get started.");
        listStack->addWidget(list);
        listStack->addWidget(emptyState);
        listStack->setCurrentWidget(emptyState);
        layout->addWidget(listStack, 1);

        auto *actions = new QWidget;
        actionsLayout = new QBoxLayout(QBoxLayout::LeftToRight, actions);
        actionsLayout->setContentsMargins(0, 0, 0, 0);
        actionsLayout->setSpacing(8);
        auto *addButton = Theme::button("Add entry", "soft");
        addButton->setObjectName(key + "Add");
        editButton = Theme::button("Edit", "soft");
        editButton->setObjectName(key + "Edit");
        removeButton = Theme::button("Remove", "danger");
        removeButton->setObjectName(key + "Remove");
        for (QPushButton *button : { addButton, editButton, removeButton }) {
            button->setMinimumHeight(42);
            button->setMinimumWidth(0);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            actionsLayout->addWidget(button, 1);
        }
        editButton->setEnabled(false);
        removeButton->setEnabled(false);
        layout->addWidget(actions);

        connect(list, &QListWidget::currentRowChanged, this, [this](int row) {
            editButton->setEnabled(row >= 0 && row < values.size());
            removeButton->setEnabled(row >= 0 && row < values.size());
        });
        connect(addButton, &QPushButton::clicked, this, [this]() { addEntry(); });
        connect(editButton, &QPushButton::clicked, this, [this]() { editEntry(); });
        connect(removeButton, &QPushButton::clicked, this, [this]() { removeEntry(); });
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        if (actionsLayout)
            actionsLayout->setDirection(width() < 390 ? QBoxLayout::TopToBottom
                                                      : QBoxLayout::LeftToRight);
        QTimer::singleShot(0, this, [this]() { updateRowHeights(); });
    }

public:
    QVector<ResumeItem> items() const { return values; }

    void setItems(const QVector<ResumeItem> &items) {
        values = items;
        refreshList();
    }

    void setOnChanged(std::function<void(const QVector<ResumeItem> &)> callback) {
        onChanged = std::move(callback);
    }

private:
    QString summary(const ResumeItem &item) const {
        QStringList lines;
        if (!item.title.isEmpty()) lines.append(item.title);
        QStringList metadata;
        if (!item.subtitle.isEmpty()) metadata.append(item.subtitle);
        if (!item.detail.isEmpty()) metadata.append(item.detail);
        if (!item.year.isEmpty()) metadata.append(item.year);
        if (!item.score.isEmpty()) metadata.append("CGPA " + item.score);
        if (!item.technologies.isEmpty()) metadata.append(item.technologies);
        if (!metadata.isEmpty()) lines.append(metadata.join(" | "));
        if (!item.description.isEmpty()) lines.append(item.description);
        if (!item.link.isEmpty()) lines.append(item.link);
        return lines.join('\n');
    }

    void refreshList() {
        list->clear();
        for (const ResumeItem &item : values) {
            auto *row = new QListWidgetItem(summary(item), list);
            row->setToolTip(summary(item));
        }
        updateRowHeights();
        listStack->setCurrentWidget(values.isEmpty() ? emptyState : list);
        if (!values.isEmpty()) list->setCurrentRow(values.size() - 1);
        else list->setCurrentRow(-1);
        updateButtons();
    }

    void updateRowHeights() {
        const QFontMetrics metrics(list->font());
        const int textWidth = qMax(100, list->viewport()->width() - 24);
        for (int row = 0; row < list->count(); ++row) {
            QListWidgetItem *item = list->item(row);
            const QRect bounds = metrics.boundingRect(QRect(0, 0, textWidth, 1000),
                                                       Qt::TextWordWrap, item->text());
            item->setSizeHint(QSize(0, qMax(48, bounds.height() + 16)));
        }
    }

    void updateButtons() {
        const int row = list->currentRow();
        editButton->setEnabled(row >= 0 && row < values.size());
        removeButton->setEnabled(row >= 0 && row < values.size());
    }

    void addEntry() {
        ResumeRecordDialog dialog(type, {}, this);
        if (dialog.exec() != QDialog::Accepted) return;
        values.append(dialog.record());
        refreshList();
        notifyChanged();
    }

    void editEntry() {
        const int row = list->currentRow();
        if (row < 0 || row >= values.size()) return;
        ResumeRecordDialog dialog(type, values[row], this);
        if (dialog.exec() != QDialog::Accepted) return;
        values[row] = dialog.record();
        refreshList();
        list->setCurrentRow(row);
        notifyChanged();
    }

    void removeEntry() {
        const int row = list->currentRow();
        if (row < 0 || row >= values.size()) return;
        values.removeAt(row);
        refreshList();
        notifyChanged();
    }

    void notifyChanged() {
        if (onChanged) onChanged(values);
    }
};

class ResumeStudioPage : public Theme::Page {
    Ctx ctx;
    QString studentEmail;
    ResumeRepository repository;
    ResumeData data;
    bool loading = false;
    bool dirty = false;
    bool responsiveLayoutInitialized = false;
    enum class LayoutMode { Wide, Compact, Stacked };
    LayoutMode layoutMode = LayoutMode::Stacked;

    QSplitter *workspaceSplitter = nullptr;
    QWidget *navigationPanel = nullptr;
    QWidget *compactNavigationRow = nullptr;
    QWidget *editorPanel = nullptr;
    QWidget *previewPanel = nullptr;
    QListWidget *sectionNav = nullptr;
    QComboBox *sectionSelect = nullptr;
    QStackedWidget *sectionStack = nullptr;
    QLabel *sectionTitle = nullptr;
    QLabel *sectionDescription = nullptr;
    QBoxLayout *headerLayout = nullptr;
    QBoxLayout *actionLayout = nullptr;
    QTextBrowser *preview = nullptr;
    QLabel *scoreLabel = nullptr;
    QLabel *scoreHint = nullptr;
    QProgressBar *scoreBar = nullptr;
    QComboBox *templateCombo = nullptr;
    QLabel *statusLabel = nullptr;
    QLineEdit *nameEdit = nullptr;
    QLineEdit *emailEdit = nullptr;
    QLineEdit *phoneEdit = nullptr;
    QLineEdit *locationEdit = nullptr;
    QLineEdit *linkedinEdit = nullptr;
    QLineEdit *githubEdit = nullptr;
    QTextEdit *skillsEdit = nullptr;
    QTextEdit *objectiveEdit = nullptr;
    QStringList sectionTitles;
    QStringList sectionDescriptions;
    ResumeListEditor *educationEditor = nullptr;
    ResumeListEditor *projectsEditor = nullptr;
    ResumeListEditor *experienceEditor = nullptr;
    ResumeListEditor *certificationsEditor = nullptr;
    ResumeListEditor *achievementsEditor = nullptr;
    ResumeListEditor *activitiesEditor = nullptr;

public:
    explicit ResumeStudioPage(const Ctx &context, const QString &storageDirectory = QString(),
                              QWidget *parent = nullptr)
        : Theme::Page(parent), ctx(context), repository(storageDirectory) {
        if (const auto *student = dynamic_cast<const Student *>(ctx.user))
            studentEmail = student->getEmail();
        build();
        loadSavedResume();
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        Theme::Page::resizeEvent(event);
        updateResponsiveLayout();
    }

private:
    QWidget *field(const QString &labelText, QLineEdit *edit, const QString &placeholder,
                   const QString &objectName) {
        auto *box = new QWidget;
        auto *layout = new QVBoxLayout(box);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(5);
        auto *label = Theme::label(labelText.toUpper(), "eyebrow");
        edit->setObjectName(objectName);
        edit->setPlaceholderText(placeholder);
        edit->setFixedHeight(42);
        layout->addWidget(label);
        layout->addWidget(edit);
        return box;
    }

    QWidget *textField(const QString &labelText, QTextEdit *edit, const QString &placeholder,
                       const QString &objectName, int minimumHeight) {
        auto *box = new QWidget;
        auto *layout = new QVBoxLayout(box);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(5);
        auto *label = Theme::label(labelText.toUpper(), "eyebrow");
        edit->setObjectName(objectName);
        edit->setPlaceholderText(placeholder);
        edit->setMinimumHeight(minimumHeight);
        edit->setAcceptRichText(false);
        edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        layout->addWidget(label);
        layout->addWidget(edit);
        return box;
    }

    void build() {
        lay->setSpacing(14);

        auto *header = new QWidget;
        headerLayout = new QBoxLayout(QBoxLayout::LeftToRight, header);
        headerLayout->setContentsMargins(0, 0, 0, 0);
        headerLayout->setSpacing(14);
        auto *titleBlock = new QVBoxLayout;
        titleBlock->setContentsMargins(0, 0, 0, 0);
        titleBlock->setSpacing(3);
        auto *title = Theme::label("Resume Studio", "h2");
        title->setObjectName("resumePageTitle");
        auto *subtitle = Theme::label(
            "Build an ATS-friendly resume, preview it live, and export a polished PDF.",
            "muted", nullptr, true);
        subtitle->setObjectName("resumePageDescription");
        titleBlock->addWidget(title);
        titleBlock->addWidget(subtitle);
        headerLayout->addLayout(titleBlock, 1);

        auto *actions = new QWidget;
        actionLayout = new QBoxLayout(QBoxLayout::LeftToRight, actions);
        actionLayout->setContentsMargins(0, 0, 0, 0);
        actionLayout->setSpacing(8);
        auto *saveButton = Theme::button("Save resume", "primary");
        saveButton->setObjectName("saveResume");
        saveButton->setMinimumSize(140, 44);
        saveButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        auto *pdfButton = Theme::button("Export to PDF", "soft");
        pdfButton->setObjectName("exportResumePdf");
        pdfButton->setMinimumSize(140, 44);
        pdfButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        actionLayout->addWidget(saveButton, 1);
        actionLayout->addWidget(pdfButton, 1);
        headerLayout->addWidget(actions);
        lay->addWidget(header);

        // Resume strength: the score and the single most useful next step, so the
        // student can see what a recruiter would find missing.
        auto *scoreRow = new QWidget;
        auto *scoreLayout = new QHBoxLayout(scoreRow);
        scoreLayout->setContentsMargins(0, 0, 0, 0);
        scoreLayout->setSpacing(12);
        auto *scoreText = new QVBoxLayout;
        scoreText->setSpacing(2);
        scoreLabel = Theme::label("Resume strength", "h3");
        scoreLabel->setObjectName("resumeScore");
        scoreHint = Theme::label("", "muted", nullptr, true);
        scoreHint->setObjectName("resumeScoreHint");
        scoreHint->setWordWrap(true);
        scoreText->addWidget(scoreLabel);
        scoreText->addWidget(scoreHint);
        scoreLayout->addLayout(scoreText, 1);
        scoreBar = Theme::bar(0, "forest");
        scoreBar->setObjectName("resumeScoreBar");
        scoreBar->setFixedWidth(150);
        scoreLayout->addWidget(scoreBar, 0, Qt::AlignVCenter);
        lay->addWidget(scoreRow);

        statusLabel = Theme::label("Changes are saved on this device only when you choose Save resume.",
                                   "muted", nullptr, true);
        statusLabel->setObjectName("resumeStatus");
        statusLabel->setWordWrap(true);
        lay->addWidget(statusLabel);
        connect(saveButton, &QPushButton::clicked, this, [this]() { saveResume(); });
        connect(pdfButton, &QPushButton::clicked, this, [this]() { exportPdf(); });

        navigationPanel = Theme::card();
        navigationPanel->setObjectName("resumeNavigationPanel");
        navigationPanel->setMinimumWidth(180);
        navigationPanel->setMaximumWidth(260);
        auto *navigationLayout = Theme::vbox(navigationPanel, 16, 12);
        navigationLayout->addWidget(Theme::sectionHead(
            "Resume sections", "Select a section to edit."));
        sectionNav = new QListWidget;
        sectionNav->setObjectName("resumeSectionNav");
        sectionNav->setSelectionMode(QAbstractItemView::SingleSelection);
        sectionNav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        sectionNav->setWordWrap(true);
        sectionNav->setSpacing(4);
        sectionNav->setUniformItemSizes(true);
        navigationLayout->addWidget(sectionNav, 1);

        editorPanel = Theme::card();
        editorPanel->setObjectName("resumeEditorPanel");
        editorPanel->setMinimumWidth(360);
        auto *editorLayout = Theme::vbox(editorPanel, 16, 12);

        compactNavigationRow = new QWidget;
        compactNavigationRow->setObjectName("resumeCompactNavigation");
        auto *compactLayout = new QHBoxLayout(compactNavigationRow);
        compactLayout->setContentsMargins(0, 0, 0, 0);
        compactLayout->setSpacing(10);
        compactLayout->addWidget(Theme::label("SECTION", "eyebrow"));
        sectionSelect = new QComboBox;
        sectionSelect->setObjectName("resumeSectionSelect");
        sectionSelect->setMinimumWidth(0);
        sectionSelect->setFixedHeight(42);
        compactLayout->addWidget(sectionSelect, 1);
        editorLayout->addWidget(compactNavigationRow);

        sectionTitle = Theme::label("", "h2");
        sectionTitle->setObjectName("resumeSectionTitle");
        sectionDescription = Theme::label("", "muted", nullptr, true);
        sectionDescription->setObjectName("resumeSectionDescription");
        sectionDescription->setWordWrap(true);
        editorLayout->addWidget(sectionTitle);
        editorLayout->addWidget(sectionDescription);

        sectionStack = new QStackedWidget;
        sectionStack->setObjectName("resumeEditorStack");
        sectionStack->setMinimumWidth(0);
        auto addSection = [this](const QString &heading, const QString &description,
                                 QWidget *page) {
            sectionTitles.append(heading);
            sectionDescriptions.append(description);
            auto *item = new QListWidgetItem(heading, sectionNav);
            item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            item->setSizeHint(QSize(0, 54));
            sectionSelect->addItem(heading);
            sectionStack->addWidget(page);
        };
        addSection("Personal Information", "Add your contact details and professional links.",
                   buildPersonalTab());
        addSection("Career Summary", "Write a concise objective or professional summary.",
                   buildSummaryTab());
        addSection("Education", "Add institutions, degrees and academic results.",
                   buildEducationTab());
        addSection("Technical Skills", "List programming languages, tools and technologies.",
                   buildSkillsTab());
        addSection("Projects", "Show your strongest projects and the work behind them.",
                   buildProjectsTab());
        addSection("Experience", "Include internships, employment and relevant experience.",
                   buildExperienceTab());
        addSection("Certifications & Achievements",
                   "Highlight credentials, awards and measurable accomplishments.",
                   buildMoreTab());
        addSection("Activities", "Add leadership, clubs, volunteering and other activities.",
                   buildActivitiesTab());
        editorLayout->addWidget(sectionStack, 1);

        previewPanel = Theme::card();
        previewPanel->setObjectName("resumePreviewPanel");
        previewPanel->setMinimumWidth(360);
        auto *previewLayout = Theme::vbox(previewPanel, 16, 12);
        previewLayout->addWidget(Theme::sectionHead("Live preview",
            "This template is also used for PDF export."));
        auto *templateRow = new QHBoxLayout;
        templateRow->setSpacing(10);
        templateRow->addWidget(Theme::label("TEMPLATE", "eyebrow"));
        templateCombo = new QComboBox;
        templateCombo->setObjectName("resumeTemplate");
        templateCombo->addItems({ "Modern", "Classic", "Minimal" });
        templateCombo->setFixedHeight(42);
        templateRow->addWidget(templateCombo, 1);
        previewLayout->addLayout(templateRow);
        auto *previewCanvas = new QFrame;
        previewCanvas->setObjectName("resumePreviewCanvas");
        auto *canvasLayout = new QVBoxLayout(previewCanvas);
        canvasLayout->setContentsMargins(10, 10, 10, 10);
        canvasLayout->setSpacing(0);
        preview = new QTextBrowser;
        preview->setObjectName("resumePreview");
        preview->setOpenExternalLinks(true);
        preview->setFrameShape(QFrame::NoFrame);
        preview->setMinimumHeight(480);
        preview->setMinimumWidth(0);
        preview->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        canvasLayout->addWidget(preview, 1);
        previewLayout->addWidget(previewCanvas, 1);

        workspaceSplitter = new QSplitter(Qt::Horizontal);
        workspaceSplitter->setObjectName("resumeWorkspaceSplitter");
        workspaceSplitter->setChildrenCollapsible(false);
        workspaceSplitter->setHandleWidth(8);
        workspaceSplitter->addWidget(navigationPanel);
        workspaceSplitter->addWidget(editorPanel);
        workspaceSplitter->addWidget(previewPanel);
        workspaceSplitter->setStretchFactor(0, 0);
        workspaceSplitter->setStretchFactor(1, 1);
        workspaceSplitter->setStretchFactor(2, 1);
        lay->addWidget(workspaceSplitter, 1);

        connect(sectionNav, &QListWidget::currentRowChanged, this,
                [this](int index) { showSection(index); });
        connect(sectionSelect, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](int index) { showSection(index); });
        sectionNav->setCurrentRow(0);
        updateResponsiveLayout();

        connect(templateCombo, &QComboBox::currentTextChanged, this, [this](const QString &name) {
            if (loading) return;
            data.templateName = name;
            markChanged();
        });
    }

    void showSection(int index) {
        if (index < 0 || index >= sectionTitles.size()) return;
        sectionStack->setCurrentIndex(index);
        sectionTitle->setText(sectionTitles[index]);
        sectionDescription->setText(sectionDescriptions[index]);
        const QSignalBlocker navBlocker(sectionNav);
        const QSignalBlocker selectBlocker(sectionSelect);
        sectionNav->setCurrentRow(index);
        sectionSelect->setCurrentIndex(index);
    }

    void updateResponsiveLayout() {
        if (!workspaceSplitter) return;
        const int pageWidth = width();
        const LayoutMode nextMode = pageWidth >= 1080 ? LayoutMode::Wide
                                  : pageWidth >= 900 ? LayoutMode::Compact
                                                     : LayoutMode::Stacked;
        const bool modeChanged = !responsiveLayoutInitialized || nextMode != layoutMode;
        layoutMode = nextMode;
        responsiveLayoutInitialized = true;
        navigationPanel->setVisible(nextMode == LayoutMode::Wide);
        compactNavigationRow->setVisible(nextMode != LayoutMode::Wide);
        workspaceSplitter->setOrientation(nextMode == LayoutMode::Stacked
                                          ? Qt::Vertical : Qt::Horizontal);
        headerLayout->setDirection(pageWidth < 720 ? QBoxLayout::TopToBottom
                                                   : QBoxLayout::LeftToRight);
        actionLayout->setDirection(pageWidth < 390 ? QBoxLayout::TopToBottom
                                                   : QBoxLayout::LeftToRight);

        if (!modeChanged) return;
        const int splitterWidth = qMax(320, workspaceSplitter->width());
        if (nextMode == LayoutMode::Wide) {
            const int navWidth = qBound(190, splitterWidth * 18 / 100, 250);
            const int remaining = qMax(760, splitterWidth - navWidth - 16);
            const int editWidth = remaining * 47 / 100;
            workspaceSplitter->setSizes({ navWidth, editWidth, remaining - editWidth });
        } else if (nextMode == LayoutMode::Compact) {
            const int editWidth = splitterWidth * 49 / 100;
            workspaceSplitter->setSizes({ 0, editWidth, splitterWidth - editWidth });
        } else {
            workspaceSplitter->setSizes({ 0, 480, 520 });
        }
    }

    QWidget *scrollableSection(QWidget *content) {
        content->setMinimumWidth(0);
        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setWidget(content);
        return scroll;
    }

    QVBoxLayout *sectionLayout(QWidget *content) {
        auto *layout = new QVBoxLayout(content);
        layout->setContentsMargins(2, 2, 8, 8);
        layout->setSpacing(14);
        return layout;
    }

    QWidget *buildPersonalTab() {
        auto *content = new QWidget;
        auto *layout = sectionLayout(content);
        auto *personal = new Theme::ResponsiveGrid(205, 12);
        nameEdit = new QLineEdit;
        emailEdit = new QLineEdit;
        phoneEdit = new QLineEdit;
        locationEdit = new QLineEdit;
        linkedinEdit = new QLineEdit;
        githubEdit = new QLineEdit;
        personal->addWidget(field("Full name", nameEdit, "Your name", "resumeName"));
        personal->addWidget(field("Email", emailEdit, "name@example.com", "resumeEmail"));
        personal->addWidget(field("Phone", phoneEdit, "+1 555 0100", "resumePhone"));
        personal->addWidget(field("Location", locationEdit, "City, Country", "resumeLocation"));
        personal->addWidget(field("LinkedIn", linkedinEdit, "https://linkedin.com/in/...", "resumeLinkedIn"));
        personal->addWidget(field("GitHub", githubEdit, "https://github.com/...", "resumeGitHub"));
        layout->addWidget(personal);
        auto connectLine = [this](QLineEdit *edit) {
            connect(edit, &QLineEdit::textChanged, this, [this]() { markChanged(); });
        };
        for (QLineEdit *edit : { nameEdit, emailEdit, phoneEdit, locationEdit,
                                  linkedinEdit, githubEdit })
            connectLine(edit);
        layout->addStretch(1);
        return scrollableSection(content);
    }

    QWidget *buildSummaryTab() {
        auto *content = new QWidget;
        auto *layout = sectionLayout(content);
        objectiveEdit = new QTextEdit;
        layout->addWidget(textField("Professional summary", objectiveEdit,
            "Summarize your strengths, experience and career direction in a few clear sentences.",
            "resumeObjective", 164));
        connect(objectiveEdit, &QTextEdit::textChanged, this, [this]() { markChanged(); });
        layout->addStretch(1);
        return scrollableSection(content);
    }

    QWidget *buildSkillsTab() {
        auto *content = new QWidget;
        auto *layout = sectionLayout(content);
        skillsEdit = new QTextEdit;
        layout->addWidget(textField("Technical skills", skillsEdit,
            "Programming languages, tools and technologies. Separate items with commas or use one line per skill.",
            "resumeSkills", 148));
        connect(skillsEdit, &QTextEdit::textChanged, this, [this]() { markChanged(); });
        layout->addStretch(1);
        return scrollableSection(content);
    }

    QWidget *buildEducationTab() {
        auto *content = new QWidget;
        auto *layout = sectionLayout(content);
        educationEditor = new ResumeListEditor(ResumeRecordType::Education, "education",
            QString(), QString());
        layout->addWidget(educationEditor, 1);
        educationEditor->setOnChanged([this](const QVector<ResumeItem> &items) {
            data.education = items;
            markChanged();
        });
        return scrollableSection(content);
    }

    QWidget *buildProjectsTab() {
        auto *content = new QWidget;
        auto *layout = sectionLayout(content);
        projectsEditor = new ResumeListEditor(ResumeRecordType::Project, "project",
            QString(), QString());
        layout->addWidget(projectsEditor, 1);
        projectsEditor->setOnChanged([this](const QVector<ResumeItem> &items) {
            data.projects = items;
            markChanged();
        });
        return scrollableSection(content);
    }

    QWidget *buildExperienceTab() {
        auto *content = new QWidget;
        auto *layout = sectionLayout(content);
        experienceEditor = new ResumeListEditor(ResumeRecordType::Experience, "experience",
            QString(), QString());
        layout->addWidget(experienceEditor, 1);
        experienceEditor->setOnChanged([this](const QVector<ResumeItem> &items) {
            data.experience = items;
            markChanged();
        });
        return scrollableSection(content);
    }

    QWidget *buildMoreTab() {
        auto *content = new QWidget;
        auto *layout = sectionLayout(content);
        certificationsEditor = new ResumeListEditor(ResumeRecordType::Certification,
            "certification", QString(), QString());
        achievementsEditor = new ResumeListEditor(ResumeRecordType::Achievement,
            "achievement", QString(), QString());
        layout->addWidget(certificationsEditor);
        layout->addWidget(achievementsEditor);
        certificationsEditor->setOnChanged([this](const QVector<ResumeItem> &items) {
            data.certifications = items;
            markChanged();
        });
        achievementsEditor->setOnChanged([this](const QVector<ResumeItem> &items) {
            data.achievements = items;
            markChanged();
        });
        layout->addStretch(1);
        return scrollableSection(content);
    }

    QWidget *buildActivitiesTab() {
        auto *content = new QWidget;
        auto *layout = sectionLayout(content);
        activitiesEditor = new ResumeListEditor(ResumeRecordType::Activity,
            "activity", QString(), QString());
        layout->addWidget(activitiesEditor, 1);
        activitiesEditor->setOnChanged([this](const QVector<ResumeItem> &items) {
            data.activities = items;
            markChanged();
        });
        return scrollableSection(content);
    }

    void loadSavedResume() {
        const auto *student = dynamic_cast<const Student *>(ctx.user);
        if (!student || studentEmail.isEmpty()) {
            lay->addWidget(Theme::label("Resume Studio is available to signed-in students.", "error", nullptr, true));
            return;
        }
        bool found = false;
        const QString error = repository.load(studentEmail, &data, &found);
        if (!error.isEmpty()) {
            data = ResumeData();
            data.fullName = student->getName();
            data.email = student->getEmail();
            data.technicalSkills = student->getSkills();
            populateEditor();
            setStatus(error, true);
            return;
        }
        if (!found) {
            data.fullName = student->getName();
            data.email = student->getEmail();
            data.technicalSkills = student->getSkills();
        }
        if (data.templateName != "Modern" && data.templateName != "Classic" &&
            data.templateName != "Minimal") data.templateName = "Modern";
        populateEditor();
        setStatus(found ? "Saved resume loaded from this device."
                        : "Start with your student profile details, then add your experience.", false);
    }

    void populateEditor() {
        loading = true;
        nameEdit->setText(data.fullName);
        emailEdit->setText(data.email);
        phoneEdit->setText(data.phone);
        locationEdit->setText(data.location);
        linkedinEdit->setText(data.linkedin);
        githubEdit->setText(data.github);
        objectiveEdit->setPlainText(data.objective);
        skillsEdit->setPlainText(data.technicalSkills);
        {
            const QSignalBlocker blocker(templateCombo);
            templateCombo->setCurrentText(data.templateName);
        }
        educationEditor->setItems(data.education);
        projectsEditor->setItems(data.projects);
        experienceEditor->setItems(data.experience);
        certificationsEditor->setItems(data.certifications);
        achievementsEditor->setItems(data.achievements);
        activitiesEditor->setItems(data.activities);
        loading = false;
        dirty = false;
        refreshPreview();
    }

    void collectScalarFields() {
        data.fullName = nameEdit->text().trimmed();
        data.email = emailEdit->text().trimmed();
        data.phone = phoneEdit->text().trimmed();
        data.location = locationEdit->text().trimmed();
        data.linkedin = linkedinEdit->text().trimmed();
        data.github = githubEdit->text().trimmed();
        data.objective = objectiveEdit->toPlainText().trimmed();
        data.technicalSkills = skillsEdit->toPlainText().trimmed();
        data.templateName = templateCombo->currentText();
    }

    void markChanged() {
        if (loading) return;
        collectScalarFields();
        dirty = true;
        setStatus("Unsaved changes", false);
        refreshPreview();
    }

    void refreshPreview() {
        if (preview) preview->setHtml(ResumeDocumentRenderer::html(data));
        refreshScore();
    }

    void refreshScore() {
        if (!scoreLabel || !scoreBar || !scoreHint) return;
        const ResumeScore score = ResumeScorer::evaluate(data);
        scoreLabel->setText(QString("Resume strength  %1%").arg(score.percent));
        scoreBar->setValue(score.percent);
        scoreBar->setProperty("tone", score.percent >= 80 ? "forest"
                                     : score.percent >= 50 ? "amber" : "danger");
        scoreBar->style()->unpolish(scoreBar);
        scoreBar->style()->polish(scoreBar);
        if (score.missing.isEmpty()) {
            scoreHint->setText("Every section a campus recruiter looks for is filled in.");
            scoreBar->setToolTip("Nothing left to add.");
            return;
        }
        scoreHint->setText("Next: " + score.missing.first());
        scoreBar->setToolTip("Still missing:\n\u2022 " + score.missing.join("\n\u2022 "));
    }

    void setStatus(const QString &message, bool isError) {
        if (!statusLabel) return;
        statusLabel->setText(message);
        statusLabel->setProperty("role", isError ? "error" : "muted");
        statusLabel->style()->unpolish(statusLabel);
        statusLabel->style()->polish(statusLabel);
    }

    void saveResume() {
        collectScalarFields();
        if (data.fullName.isEmpty()) {
            setStatus("Enter your full name before saving.", true);
            nameEdit->setFocus();
            return;
        }
        const QString emailError = Validator::emailError(data.email);
        if (!emailError.isEmpty()) {
            setStatus(emailError, true);
            emailEdit->setFocus();
            return;
        }
        const QString error = repository.save(studentEmail, data);
        if (!error.isEmpty()) {
            setStatus(error, true);
            return;
        }
        dirty = false;
        setStatus("Resume saved on this device.", false);
        refreshPreview();
    }

    void exportPdf() {
        collectScalarFields();
        QString path = QFileDialog::getSaveFileName(this, "Export resume to PDF",
            QDir::homePath() + "/PlaceBridge-Resume.pdf", "PDF files (*.pdf)");
        if (path.isEmpty()) return;
        if (!path.endsWith(".pdf", Qt::CaseInsensitive)) path += ".pdf";
        const QString error = ResumePdfExporter::exportFile(path, data);
        if (!error.isEmpty()) {
            setStatus(error, true);
            return;
        }
        setStatus("PDF exported: " + path, false);
    }
};

#endif // RESUME_STUDIO_H
