#include "settings_page.h"
#include "dialogs.h"
#include "SystemServices.h"

#include <QTemporaryDir>
#include <QInputDialog>
#include <QHostAddress>
#include <QNetworkReply>
#include <algorithm>
#include <QtCore/private/qzipreader_p.h>
#include <QtCore/private/qzipwriter_p.h>

namespace {
}

SettingsPage::SettingsPage(const Ctx &context) : ctx(context) {
    lay->addWidget(Theme::hero("Personalize PlaceBridge", "Settings",
                               "Appearance, accessibility, data location, backup and application information."));

    auto *appearance = Theme::card();
    auto *av = Theme::vbox(appearance, 22, 14);
    av->addWidget(Theme::sectionHead("Appearance", "Choose a color preset, mode and readable text size."));
    auto *form = new QFormLayout;
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    auto *preset = new QComboBox;
    preset->addItems({ "Forest+Amber", "Teal", "Navy+Amber" });
    preset->setCurrentText(QSettings().value("theme/preset", "Forest+Amber").toString());
    auto *mode = new QComboBox;
    mode->addItems({ "Light", "Dark" });
    mode->setCurrentText(QSettings().value("theme/mode", "Light").toString());
    auto *fontSize = new QComboBox;
    fontSize->addItems({ "Small", "Medium", "Large" });
    fontSize->setCurrentText(QSettings().value("theme/fontSize", "Medium").toString());
    form->addRow("Color preset", preset);
    form->addRow("Theme", mode);
    form->addRow("Font size", fontSize);
    auto *networked = new QCheckBox("Use local REST server for sign-in");
    networked->setChecked(QSettings().value("network/enabled", false).toBool());
    auto *endpoint = new QLineEdit(QSettings().value("network/endpoint",
                                                       "http://127.0.0.1:8080").toString());
    auto *probe = Theme::button("Test server", "soft");
    form->addRow("REST sign-in", networked);
    form->addRow("Local server URL", endpoint);
    form->addRow("", Theme::label(
        "The API binds to localhost. Desktop data remains in the configured SQLite database.",
        "muted", nullptr, true));
    form->addRow("", probe);
    av->addLayout(form);
    auto *apply = Theme::button("Apply appearance", "primary");
    av->addWidget(apply, 0, Qt::AlignRight);
    connect(apply, &QPushButton::clicked, this,
            [preset, mode, fontSize, networked, endpoint]() {
        QSettings settings;
        settings.setValue("theme/preset", preset->currentText());
        settings.setValue("theme/mode", mode->currentText());
        settings.setValue("theme/fontSize", fontSize->currentText());
        settings.setValue("network/enabled", networked->isChecked());
        settings.setValue("network/endpoint", endpoint->text().trimmed());
        Theme::refresh(*qApp);
        QMessageBox::information(nullptr, "Appearance updated", "Settings saved.");
    });
    connect(probe, &QPushButton::clicked, this, [this, endpoint]() {
        const QUrl url(endpoint->text().trimmed());
        QHostAddress address;
        const bool loopback = url.host() == "localhost" ||
            (address.setAddress(url.host()) && address.isLoopback());
        if (!url.isValid() || url.scheme() != "http" || !loopback) {
            QMessageBox::warning(this, "Invalid server URL",
                "Use a localhost address such as http://127.0.0.1:8080. Remote HTTP authentication is disabled.");
            return;
        }
        auto *client = new RestClient(url, this);
        QNetworkReply *reply = client->get("/api/health");
        connect(reply, &QNetworkReply::finished, this, [this, client, reply]() {
            const bool ready = reply->error() == QNetworkReply::NoError &&
                               reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200;
            const QString error = reply->errorString();
            reply->deleteLater();
            client->deleteLater();
            if (ready) QMessageBox::information(this, "Server available", "PlaceBridge REST API is responding.");
            else QMessageBox::warning(this, "Server unavailable", "Could not reach the REST API: " + error);
        });
    });
    lay->addWidget(appearance);

    // MainWindow stores this per role under navigation/<role>/visible; reading the
    // old global key here meant the checkbox never reflected the real setting.
    const QString navigationKey = ctx.user
        ? QString("navigation/%1/visible").arg(ctx.user->role())
        : QStringLiteral("navigation/visible");
    auto *navigation = new QCheckBox("Show navigation tabs");
    navigation->setChecked(QSettings().value(navigationKey, true).toBool());
    connect(navigation, &QCheckBox::toggled, this, [this](bool visible) {
        if (ctx.setNavigationVisible) ctx.setNavigationVisible(visible);
    });
    auto *navCard = Theme::card();
    auto *nv = Theme::vbox(navCard, 20, 10);
    nv->addWidget(Theme::sectionHead("Navigation", "Hide or show the role-specific navigation buttons."));
    nv->addWidget(navigation);
    lay->addWidget(navCard);

    auto *dataCard = Theme::card();
    auto *dv = Theme::vbox(dataCard, 22, 12);
    dv->addWidget(Theme::sectionHead("Database and backups",
        "The active SQLite database is stored in the selected folder. Backups are ZIP files."));
    auto *pathRow = new QHBoxLayout;
    auto *path = new QLineEdit(ctx.store->dataFolder());
    path->setReadOnly(true);
    path->setMinimumWidth(0);
    path->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto *browse = Theme::button("Browse", "soft");
    pathRow->addWidget(path, 1);
    pathRow->addWidget(browse);
    dv->addLayout(pathRow);
    const bool admin = ctx.store->adminActor();
    const bool coordinator = ctx.user &&
        Permissions::isCoordinator(ctx.user->role());
    browse->setEnabled(admin);
    connect(browse, &QPushButton::clicked, this, [this, path]() {
        const QString selected = QFileDialog::getExistingDirectory(this, "Choose data folder", path->text());
        if (selected.isEmpty()) return;
        if (!ctx.store->changeDataFolder(selected)) {
            QMessageBox::critical(this, "Data folder change failed", ctx.store->lastError());
            return;
        }
        path->setText(ctx.store->dataFolder());
        Theme::toast(this, "Database moved safely");
    });
    auto *backup = Theme::button("Create ZIP backup", "soft");
    auto *restore = Theme::button("Restore from ZIP", "danger");
    backup->setEnabled(admin);
    restore->setEnabled(admin);
    connect(backup, &QPushButton::clicked, this, [this]() {
        const QString archive = QFileDialog::getSaveFileName(this, "Create PlaceBridge backup",
            QDir::homePath() + "/placebridge-backup.zip", "ZIP archive (*.zip)");
        if (archive.isEmpty()) return;
        const QString database = QFileInfo(ctx.store->databasePath()).fileName();
        QFile input(ctx.store->databasePath());
        if (!input.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(this, "Backup failed", input.errorString());
            return;
        }
        QZipWriter zip(archive);
        zip.addFile(database, input.readAll());
        zip.close();
        if (zip.status() != QZipWriter::NoError) {
            QMessageBox::critical(this, "Backup failed", "Could not write ZIP backup.");
            return;
        }
        Theme::toast(this, "Backup saved");
    });
    connect(restore, &QPushButton::clicked, this, [this]() {
        const QString archive = QFileDialog::getOpenFileName(this, "Restore PlaceBridge backup",
                                                              QDir::homePath(), "ZIP archive (*.zip)");
        if (archive.isEmpty()) return;
        if (QMessageBox::question(this, "Restore backup",
                "Restoring replaces the current database. Continue?") != QMessageBox::Yes) return;
        QZipReader zip(archive);
        if (!zip.exists() || zip.status() != QZipReader::NoError) {
            QMessageBox::critical(this, "Restore failed", "Could not open this ZIP archive.");
            return;
        }
        const QString databaseName = QFileInfo(ctx.store->databasePath()).fileName();
        bool containsDatabase = false;
        for (const QZipReader::FileInfo &entry : zip.fileInfoList())
            if (entry.filePath == databaseName && entry.isFile && entry.size <= 512LL * 1024 * 1024)
                containsDatabase = true;
        if (!containsDatabase) {
            QMessageBox::critical(this, "Restore failed", "The ZIP does not contain a valid-sized PlaceBridge database.");
            return;
        }
        const QByteArray backupData = zip.fileData(databaseName);
        zip.close();
        if (backupData.isEmpty()) {
            QMessageBox::critical(this, "Restore failed", "Could not read the backup database.");
            return;
        }
        QTemporaryDir temporary;
        if (!temporary.isValid()) {
            QMessageBox::critical(this, "Restore failed", "Could not create a temporary restore file.");
            return;
        }
        const QString backupDb = temporary.path() + "/" + databaseName;
        QFile output(backupDb);
        if (!output.open(QIODevice::WriteOnly) || output.write(backupData) != backupData.size()) {
            QMessageBox::critical(this, "Restore failed", "Could not stage the backup database.");
            return;
        }
        output.close();
        if (!ctx.store->restoreDatabaseFromFile(backupDb)) {
            QMessageBox::critical(this, "Restore failed", ctx.store->lastError());
            return;
        }
        Theme::toast(this, "Database restored");
    });
    auto *buttons = new Theme::ResponsiveGrid(170, 8);
    buttons->addWidget(backup);
    buttons->addWidget(restore);
    dv->addWidget(buttons);
    lay->addWidget(dataCard);

    if (coordinator) {
        const auto account = ctx.store->findAccount(ctx.user->getEmail());
        const QString branch = account ? account->extra : QString();
        auto *announcementCard = Theme::card();
        auto *announcementLayout = Theme::vbox(announcementCard, 18, 12);
        announcementLayout->addWidget(Theme::sectionHead(
            "Branch announcements",
            "Send a notice only to students in your assigned branch: " + branch));
        auto *compose = new QTextEdit;
        compose->setPlaceholderText("Write a placement announcement for your branch…");
        auto *post = Theme::button("Post to my branch", "primary");
        announcementLayout->addWidget(compose);
        announcementLayout->addWidget(post, 0, Qt::AlignRight);
        connect(post, &QPushButton::clicked, this, [this, branch, compose]() {
            if (compose->toPlainText().trimmed().isEmpty()) {
                QMessageBox::warning(this, "Announcement is empty", "Enter an announcement first.");
                return;
            }
            if (!ctx.store->postAnnouncement(branch, compose->toPlainText())) {
                QMessageBox::warning(this, "Announcement not posted",
                                     "Could not post a notice to your assigned branch.");
                return;
            }
            compose->clear();
            Theme::toast(this, "Branch announcement sent");
        });
        lay->addWidget(announcementCard);
    }

    if (admin) {
        auto *accountCard = Theme::card();
        auto *accountLayout = Theme::vbox(accountCard, 18, 12);
        accountLayout->addWidget(Theme::sectionHead("Account administration",
            "Issue coordinator and Admin accounts or reset an existing account password."));
        auto *accountButtons = new Theme::ResponsiveGrid(190, 8);
        auto *coordinator = Theme::button("Create Department Coordinator", "soft");
        auto *administrator = Theme::button("Create Admin account", "soft");
        auto *recruiter = Theme::button("Create recruiter account", "soft");
        auto *resetPassword = Theme::button("Reset account password", "danger");
        auto *issueCode = Theme::button("Issue reset code", "soft");
        accountButtons->addWidget(coordinator);
        accountButtons->addWidget(administrator);
        accountButtons->addWidget(recruiter);
        accountButtons->addWidget(issueCode);
        accountButtons->addWidget(resetPassword);
        accountLayout->addWidget(accountButtons);
        connect(coordinator, &QPushButton::clicked, this, [this]() {
            bool ok = false;
            const QString name = QInputDialog::getText(this, "New coordinator",
                "Coordinator name:", QLineEdit::Normal, {}, &ok).trimmed();
            if (!ok) return;
            const QString email = QInputDialog::getText(this, "New coordinator",
                "Coordinator e-mail:", QLineEdit::Normal, {}, &ok).trimmed();
            if (!ok) return;
            const QString branch = QInputDialog::getText(this, "New coordinator",
                "Managed branch:", QLineEdit::Normal, {}, &ok).trimmed();
            if (!ok) return;
            const QString nameError = Validator::requiredTextError(name, "the coordinator name");
            if (!nameError.isEmpty()) {
                QMessageBox::warning(this, "Invalid coordinator", nameError);
                return;
            }
            if (branch.isEmpty()) {
                QMessageBox::warning(this, "Invalid coordinator", "Enter the coordinator's managed branch.");
                return;
            }
            const QString password = QInputDialog::getText(this, "New coordinator",
                "Temporary password:", QLineEdit::Password, {}, &ok);
            if (!ok) return;
            const QString emailError = Validator::emailError(email);
            if (!emailError.isEmpty()) {
                QMessageBox::warning(this, "Invalid coordinator", emailError);
                return;
            }
            const QString passwordError = Validator::passwordError(password);
            if (!passwordError.isEmpty()) {
                QMessageBox::warning(this, "Invalid coordinator", passwordError);
                return;
            }
            const QString result = ctx.store->registerAccount(
                email, password, "Department Coordinator", name, branch);
            if (!result.isEmpty()) QMessageBox::warning(this, "Account not created", result);
            else Theme::toast(this, "Coordinator account created");
        });
        connect(administrator, &QPushButton::clicked, this, [this]() {
            bool ok = false;
            const QString name = QInputDialog::getText(this, "New Admin",
                "Admin name:", QLineEdit::Normal, {}, &ok).trimmed();
            if (!ok) return;
            const QString email = QInputDialog::getText(this, "New Admin",
                "Admin e-mail:", QLineEdit::Normal, {}, &ok).trimmed();
            if (!ok) return;
            const QString employeeId = QInputDialog::getText(this, "New Admin",
                "Employee ID:", QLineEdit::Normal, {}, &ok).trimmed();
            if (!ok) return;
            const QString password = QInputDialog::getText(this, "New Admin",
                "Temporary password:", QLineEdit::Password, {}, &ok);
            if (!ok) return;
            const QString nameError = Validator::requiredTextError(name, "the Admin name");
            if (!nameError.isEmpty()) {
                QMessageBox::warning(this, "Invalid Admin", nameError);
                return;
            }
            if (employeeId.isEmpty()) {
                QMessageBox::warning(this, "Invalid Admin", "Enter the Admin's employee ID.");
                return;
            }
            const QString emailError = Validator::emailError(email);
            if (!emailError.isEmpty()) {
                QMessageBox::warning(this, "Invalid Admin", emailError);
                return;
            }
            const QString passwordError = Validator::passwordError(password);
            if (!passwordError.isEmpty()) {
                QMessageBox::warning(this, "Invalid Admin", passwordError);
                return;
            }
            const QString result = ctx.store->registerAccount(
                email, password, "Admin", name, employeeId);
            if (!result.isEmpty()) QMessageBox::warning(this, "Account not created", result);
            else Theme::toast(this, "Admin account created");
        });
        connect(recruiter, &QPushButton::clicked, this, [this]() {
            bool ok = false;
            const QString name = QInputDialog::getText(this, "New recruiter",
                "Recruiter name:", QLineEdit::Normal, {}, &ok).trimmed();
            if (!ok) return;
            const QString email = QInputDialog::getText(this, "New recruiter",
                "Recruiter e-mail:", QLineEdit::Normal, {}, &ok).trimmed();
            if (!ok) return;
            const QString companyName = QInputDialog::getText(this, "New recruiter",
                "Company name (must match an existing drive):", QLineEdit::Normal, {}, &ok).trimmed();
            if (!ok) return;
            const QVector<Company> drives = ctx.store->drives();
            const bool companyExists = std::any_of(drives.cbegin(), drives.cend(),
                [&companyName](const Company &drive) {
                    return drive.getName().compare(companyName, Qt::CaseInsensitive) == 0;
                });
            if (!companyExists) {
                QMessageBox::warning(this, "Unknown company",
                    "Create or approve a drive for this company before adding recruiter accounts.");
                return;
            }
            const QString password = QInputDialog::getText(this, "New recruiter",
                "Temporary password:", QLineEdit::Password, {}, &ok);
            if (!ok) return;
            const QString emailError = Validator::emailError(email);
            if (!emailError.isEmpty()) {
                QMessageBox::warning(this, "Invalid recruiter", emailError);
                return;
            }
            const QString passwordError = Validator::passwordError(password);
            if (!passwordError.isEmpty()) {
                QMessageBox::warning(this, "Invalid recruiter", passwordError);
                return;
            }
            const QString result = ctx.store->registerAccount(
                email, password, "Recruiter", name, companyName);
            if (!result.isEmpty()) QMessageBox::warning(this, "Account not created", result);
            else Theme::toast(this, "Recruiter account created");
        });
        connect(issueCode, &QPushButton::clicked, this, [this]() {
            IssueResetCodeDialog dialog(ctx.store, this);
            dialog.exec();
        });
        connect(resetPassword, &QPushButton::clicked, this, [this]() {
            ResetPasswordDialog dialog(ctx.store, this);
            dialog.exec();
        });
        auto *workflowButtons = new Theme::ResponsiveGrid(190, 8);
        auto *announcement = Theme::button("Post announcement", "soft");
        auto *contact = Theme::button("Track company contact / MoU", "soft");
        workflowButtons->addWidget(announcement);
        workflowButtons->addWidget(contact);
        accountLayout->addWidget(workflowButtons);
        connect(announcement, &QPushButton::clicked, this, [this]() {
            bool ok = false;
            const QString branch = QInputDialog::getText(this, "Placement announcement",
                "Branch (leave blank for all students):", QLineEdit::Normal, {}, &ok).trimmed();
            if (!ok) return;
            const QString message = QInputDialog::getMultiLineText(
                this, "Placement announcement", "Message:", {}, &ok).trimmed();
            if (!ok || message.isEmpty()) return;
            if (!ctx.store->postAnnouncement(branch, message))
                QMessageBox::warning(this, "Announcement not posted", "The announcement could not be saved.");
            else Theme::toast(this, "Announcement sent to eligible students");
        });
        connect(contact, &QPushButton::clicked, this, [this]() {
            bool ok = false;
            const QString companyName = QInputDialog::getText(this, "Company contact",
                "Company name:", QLineEdit::Normal, {}, &ok).trimmed();
            if (!ok || companyName.isEmpty()) return;
            const QString contactName = QInputDialog::getText(this, "Company contact",
                "Contact person:", QLineEdit::Normal, {}, &ok).trimmed();
            if (!ok) return;
            const QString email = QInputDialog::getText(this, "Company contact",
                "Contact e-mail:", QLineEdit::Normal, {}, &ok).trimmed();
            if (!ok) return;
            if (!email.isEmpty() && !Validator::emailError(email).isEmpty()) {
                QMessageBox::warning(this, "Invalid e-mail", Validator::emailError(email));
                return;
            }
            const QString mou = QInputDialog::getText(this, "Company contact",
                "MoU status (e.g. Active, Pending, Expired):", QLineEdit::Normal, {}, &ok).trimmed();
            if (!ok) return;
            const QString notes = QInputDialog::getMultiLineText(
                this, "Company contact", "Notes:", {}, &ok).trimmed();
            if (!ok) return;
            if (!ctx.store->updateCompanyContact(companyName, contactName, email, mou, notes))
                QMessageBox::warning(this, "Contact not saved", "Could not save company contact details.");
            else Theme::toast(this, "Company contact and MoU status saved");
        });
        const auto contacts = ctx.store->companyContacts();
        if (!contacts.isEmpty()) {
            auto *table = Theme::table({ "COMPANY", "CONTACT", "E-MAIL", "MOU STATUS", "NOTES" });
            table->setRowCount(contacts.size());
            for (int row = 0; row < contacts.size(); ++row)
                for (int column = 0; column < 5; ++column)
                    Theme::setText(table, row, column, contacts[row].value(column));
            table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
            accountLayout->addWidget(table);
        }
        lay->addWidget(accountCard);
    }

    auto *aboutButton = Theme::button("About PlaceBridge", "soft");
    connect(aboutButton, &QPushButton::clicked, this, [this]() {
        QMessageBox dialog(this);
        dialog.setWindowTitle("About PlaceBridge");
        dialog.setWindowIcon(Theme::appIcon());
        dialog.setIconPixmap(Theme::appIcon().pixmap(64, 64));
        dialog.setText("<b>PlaceBridge</b><br>Campus Placement Management<br>Version 2.0<br>"
                       "Developer: PlaceBridge Development Team");
        dialog.exec();
    });
    lay->addWidget(aboutButton, 0, Qt::AlignLeft);
    lay->addStretch();
}
