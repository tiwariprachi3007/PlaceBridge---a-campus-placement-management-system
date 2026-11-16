#include "activity_page.h"
#include "dialogs.h"

#include <QMessageBox>
#include <QTableWidget>

ActivityPage::ActivityPage(const Ctx &context) : ctx(context) {
    auto *resetPassword = Theme::button("Reset account password", "ghost");
    auto *resetDemo = Theme::button("Reset demo data", "danger");
    connect(resetPassword, &QPushButton::clicked, this, [this]() {
        ResetPasswordDialog dialog(ctx.store, this);
        if (dialog.exec() == QDialog::Accepted) Theme::toast(this, "Account password reset");
    });
    connect(resetDemo, &QPushButton::clicked, this, [this]() {
        if (QMessageBox::question(this, "Reset demo data",
                "This replaces the current records with the original demo dataset. Continue?") != QMessageBox::Yes)
            return;
        if (ctx.store->resetDemoData()) Theme::toast(this, "Demo data restored");
        else QMessageBox::critical(this, "Reset failed", ctx.store->lastError());
    });
    lay->addWidget(Theme::hero("Administrative audit trail", "Activity",
                               "A chronological record of account and placement-management actions.",
                               { resetPassword, resetDemo }));

    const auto entries = ctx.store->activityLog();
    auto *table = Theme::table({ "WHEN", "WHO", "ACTION", "DETAILS" });
    table->setRowCount(entries.size());
    for (int row = 0; row < entries.size(); ++row) {
        const ActivityEntry &entry = entries[row];
        Theme::setText(table, row, 0, entry.occurredAt.toLocalTime().toString("dd MMM yyyy, HH:mm:ss"));
        Theme::setText(table, row, 1, entry.actor);
        Theme::setText(table, row, 2, entry.action, true);
        Theme::setText(table, row, 3, entry.details);
    }
    table->setColumnWidth(0, 180);
    table->setColumnWidth(1, 230);
    table->setColumnWidth(2, 200);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    Theme::fitTable(table);
    lay->addWidget(Theme::tableCard(QString("%1 recent activity entries").arg(entries.size()),
                                    "Actions are timestamped in UTC and displayed in your local time.", nullptr, table));
    lay->addStretch();
}
