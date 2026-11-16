#include "applications_page.h"

#include <QComboBox>

ApplicationsPage::ApplicationsPage(const Ctx &context) : ctx(context) {
    lay->addWidget(Theme::hero("Placement workflow", "Applications",
                               "Review each application and update its stage. Students receive a notification when status changes."));
    const auto records = ctx.store->applications();
    auto *table = Theme::table({ "STUDENT", "COMPANY", "ROLE", "STATUS", "STAGE" });
    table->setRowCount(records.size());
    const QStringList statuses = { "Applied", "Shortlisted", "Interview", "Offer", "Rejected" };
    for (int row = 0; row < records.size(); ++row) {
        const Application application = records[row];
        Theme::setText(table, row, 0, application.student, true);
        Theme::setText(table, row, 1, application.company);
        Theme::setText(table, row, 2, application.role);

        auto *status = new QComboBox;
        status->addItems(statuses);
        if (application.status.compare("Rejected", Qt::CaseInsensitive) == 0) status->setCurrentText("Rejected");
        else if (application.stage() == 3) status->setCurrentText("Offer");
        else if (application.stage() == 2) status->setCurrentText("Interview");
        else if (application.stage() == 1) status->setCurrentText("Shortlisted");
        else status->setCurrentText("Applied");
        connect(status, &QComboBox::currentTextChanged, this, [this, application](const QString &next) {
            if (ctx.store->updateApplicationStatus(application.student, application.company, application.role, next))
                Theme::toast(this, "Application status updated");
        });
        table->setCellWidget(row, 3, status);
        const QString stage = application.stage() < 0 ? "Rejected"
                            : application.stage() == 3 ? "Offer"
                            : application.stage() == 2 ? "Interview"
                            : application.stage() == 1 ? "Shortlisted" : "Applied";
        Theme::setText(table, row, 4, stage, true);
    }
    table->setColumnWidth(0, 210);
    table->setColumnWidth(1, 160);
    table->setColumnWidth(2, 280);
    table->setColumnWidth(3, 160);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    Theme::fitTable(table);
    lay->addWidget(Theme::tableCard(QString("%1 applications").arg(records.size()),
                                    "Allowed stages: Applied, Shortlisted, Interview, Offer, Rejected.", nullptr, table));
    lay->addStretch();
}
