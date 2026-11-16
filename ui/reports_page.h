#ifndef REPORTS_PAGE_H
#define REPORTS_PAGE_H

#include "theme.h"
#include "context.h"
#include "dialogs.h"
#include "placement_officer.h"
#include "DataManager.h"
#include <QPdfWriter>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QTextDocument>
#include <QtCore/private/qzipwriter_p.h>

// Simple painted bar chart (no extra Qt module needed). Colours come from the
// active theme tokens instead of being hard-coded, so it stays legible in dark
// mode and under the Teal / Navy presets. A baseline and gridlines make the
// values readable without reading every number.
class BarChart : public QWidget {
    QVector<QPair<QString, int>> data;

public:
    explicit BarChart(const QVector<QPair<QString, int>> &d, QWidget *parent = nullptr)
        : QWidget(parent), data(d) {
        setMinimumHeight(250);
        setAccessibleName("Bar chart");
        QStringList described;
        for (const auto &entry : data)
            described << QString("%1: %2").arg(entry.first).arg(entry.second);
        setAccessibleDescription(described.join(", "));
        setToolTip(described.join('\n'));
    }

protected:
    void paintEvent(QPaintEvent *) override {
        if (data.isEmpty()) return;
        const Theme::Tokens colour = Theme::tokens();
        const QColor text(colour.text);
        const QColor muted(colour.textMuted);
        const QColor accent(colour.accent);
        const QColor border(colour.border);
        QColor bar(colour.primary);
        bar.setAlpha(palette().color(QPalette::Window).lightness() < 80 ? 150 : 70);

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        int maxV = 1;
        for (const auto &d : data) maxV = qMax(maxV, d.second);
        const QRect r = rect().adjusted(8, 30, -8, -30);
        const int n = data.size(), gap = 18;
        const int bw = qMax(6, (r.width() - gap * (n - 1)) / n);
        QFont f = font();
        f.setPixelSize(12);

        // horizontal gridlines at 0 / 50 / 100 % of the tallest bar
        p.setPen(QPen(border, 1, Qt::DotLine));
        for (int step = 0; step <= 2; ++step) {
            const int y = r.bottom() - r.height() * step / 2;
            p.drawLine(r.left(), y, r.right(), y);
        }
        p.setPen(QPen(border, 1));
        p.drawLine(r.left(), r.bottom(), r.right(), r.bottom());

        for (int i = 0; i < n; ++i) {
            const int v = data[i].second;
            const int h = int(double(v) / maxV * r.height());
            const QRect b(r.left() + i * (bw + gap), r.bottom() - h, bw, h);
            const bool peak = (v == maxV);
            p.setPen(Qt::NoPen);
            p.setBrush(peak ? accent : bar);
            p.drawRoundedRect(b, 8, 8);
            f.setBold(true);
            p.setFont(f);
            p.setPen(text);
            p.drawText(QRect(b.left() - 10, b.top() - 24, bw + 20, 20), Qt::AlignCenter,
                       QString::number(v) + (peak ? " ★" : ""));
            f.setBold(false);
            p.setFont(f);
            p.setPen(muted);
            p.drawText(QRect(b.left() - 10, r.bottom() + 8, bw + 20, 20), Qt::AlignCenter,
                       data[i].first);
        }
    }
};

// Institutional analytics, all derived from the stored records (see PlacementStats).
class ReportsPage : public Theme::Page {
    Ctx ctx;
    PlacementOfficer *officer;

public:
    explicit ReportsPage(const Ctx &c) : ctx(c), officer(dynamic_cast<PlacementOfficer *>(c.user)) { build(); }

private:
    void build() {
        const auto students = ctx.store->students();
        const auto drives = ctx.store->drives();
        const auto apps = ctx.store->applications();

        int placed = 0, offers = 0;
        for (const Student &s : students) if (s.isPlaced()) ++placed;
        for (const Application &a : apps) if (a.stage() >= 3) ++offers;
        QVector<double> pk;
        double peak = 0;
        QString peakCo;
        for (const Company &d : drives) {
            pk << d.lpa();
            if (d.lpa() > peak) { peak = d.lpa(); peakCo = d.getName(); }
        }
        std::sort(pk.begin(), pk.end());
        const double median = pk.isEmpty() ? 0 : (pk.size() % 2 ? pk[pk.size() / 2] : (pk[pk.size() / 2 - 1] + pk[pk.size() / 2]) / 2);

        auto *csvBtn = Theme::button("Export CSV", "primary");
        auto *pdfBtn = Theme::button("Export PDF Report", "ghost");
        auto *previewBtn = Theme::button("Print Preview", "ghost");
        auto *xlsxBtn = Theme::button("Export .xlsx", "ghost");
        auto *txtBtn = Theme::button("Export Summary (.txt)", "ghost");
        QObject::connect(csvBtn, &QPushButton::clicked, this, [this]() { exportCsv(); });
        QObject::connect(pdfBtn, &QPushButton::clicked, this, [this, placed]() { exportPdf(placed); });
        QObject::connect(xlsxBtn, &QPushButton::clicked, this, [this]() { exportXlsx(); });
        QObject::connect(previewBtn, &QPushButton::clicked, this, [this]() { printPreview(); });
        QObject::connect(txtBtn, &QPushButton::clicked, this, [this, placed]() { exportTxt(placed); });
        lay->addWidget(Theme::hero("Institutional Reporting Suite", "Placement Analytics",
                                   "Recruitment trends, compensation brackets, and official placement metrics.",
                                   { csvBtn, xlsxBtn, pdfBtn, previewBtn, txtBtn }));

        const int rate = students.isEmpty() ? 0 : qRound(100.0 * placed / students.size());
        auto *stats = new Theme::ResponsiveGrid(240);
        stats->addWidget(Theme::statCard("Overall Placement Rate", QString("%1%").arg(rate),
                                         QString("%1 of %2 registered students placed").arg(placed).arg(students.size())));
        stats->addWidget(Theme::statCard("Median Compensation", QString::number(median, 'f', 1) + " LPA",
                                         peak > 0 ? QString("Peak compensation: %1 (%2)").arg(Fmt::lpa(peak), peakCo) : QString("No drives yet")));
        stats->addWidget(Theme::statCard("Offers Per Student",
                                         students.isEmpty() ? "0.00" : QString::number(double(offers) / students.size(), 'f', 2),
                                         QString("%1 offers across %2 applications").arg(offers).arg(apps.size())));
        lay->addWidget(stats);

        // monthly trend + brackets
        auto *row = new Theme::ResponsiveGrid(420);

        auto *trend = Theme::card();
        auto *tv = Theme::vbox(trend, 22, 10);
        tv->addWidget(Theme::sectionHead("Placed Students by Branch", "Counted from the student records",
                                         Theme::pill("Official Records", "success")));
        const auto branchChart = PlacementStats::placedByBranchChart(students);
        if (branchChart.isEmpty()) tv->addWidget(Theme::label("No students registered yet.", "muted"));
        else tv->addWidget(new BarChart(branchChart));
        row->addWidget(trend);

        int b[4] = { 0, 0, 0, 0 };
        for (double v : pk) ++b[v < 5 ? 0 : (v < 8 ? 1 : (v < 12 ? 2 : 3))];
        const QStringList names = { "Below 5.0 LPA", "5.0 to 8.0 LPA (Standard Tier)", "8.0 to 12.0 LPA (Dream Tier)", "Above 12.0 LPA (Super Dream)" };
        auto *br = Theme::card();
        auto *bv = Theme::vbox(br, 22, 16);
        bv->addWidget(Theme::sectionHead("Compensation Package Brackets", "Share of drives by CTC bracket"));
        for (int i = 0; i < 4; ++i) {
            const int pct = pk.isEmpty() ? 0 : qRound(100.0 * b[i] / pk.size());
            bv->addWidget(Theme::metricRow(names[i], QString("%1 drive(s)").arg(b[i]), QString("%1%").arg(pct), pct, i >= 2 ? "forest" : "amber"));
        }
        bv->addStretch();
        row->addWidget(br);
        lay->addWidget(row);

        // top partners by offers
        QMap<QString, int> offersBy;
        for (const Application &a : apps) if (a.stage() >= 3) offersBy[a.company]++;
        QVector<QPair<int, QString>> top;
        for (auto it = offersBy.begin(); it != offersBy.end(); ++it) top.append({ it.value(), it.key() });
        std::sort(top.begin(), top.end(), [](const auto &x, const auto &y) { return x.first > y.first; });

        auto *pc = Theme::card();
        auto *pv = Theme::vbox(pc, 22, 14);
        pv->addWidget(Theme::sectionHead("Top Recruiting Partners by Extended Offers", "Counted from the applications file.",
                                         Theme::pill("Official Records", "success")));
        int rank = 0;
        for (const auto &t : top) {
            if (rank++ == 5) break;
            QStringList roles;
            for (const Company &d : drives) if (d.getName() == t.second && !roles.contains(d.getJobRole())) roles << d.getJobRole();
            auto *r = new QHBoxLayout;
            r->setSpacing(14);
            r->addWidget(Theme::logo(Util::initialsOf(t.second), rank % 2 == 0));
            auto *nm = new QVBoxLayout;
            nm->setSpacing(0);
            nm->addWidget(Theme::label(t.second, "h3"));
            nm->addWidget(Theme::label(roles.join(", "), "muted"));
            r->addLayout(nm, 1);
            r->addWidget(Theme::pill(QString("%1 offer%2").arg(t.first).arg(t.first == 1 ? "" : "s"), "success"));
            pv->addLayout(r);
        }
        if (top.isEmpty()) pv->addWidget(Theme::label("No offers recorded yet.", "muted"));
        lay->addWidget(pc);
        lay->addStretch();
    }

    // ------------------------------------------------------------- exports
    void exportCsv() {
        QString out = Theme::csvLine({ "Name", "E-mail", "Branch", "CGPA", "Status", "Applications", "Offers" });
        const auto apps = ctx.store->applications();
        for (const Student &s : ctx.store->students()) {
            int n = 0, o = 0;
            for (const Application &a : apps) if (a.student == s.getName()) { ++n; if (a.stage() >= 3) ++o; }
            out += Theme::csvLine({ s.getName(), s.getEmail(), s.branch(), s.getCgpa(), s.getStatus(), QString::number(n), QString::number(o) });
        }
        Theme::saveTextFile(this, "Export placement data", "placement_data.csv", "CSV files (*.csv)", out);
    }

    QString summary(int placed) const {
        if (!officer) return QString();
        return officer->summaryText(ctx.store->students().size(), placed, ctx.store->drives().size(), ctx.store->applications().size());
    }

    void exportTxt(int placed) {
        Theme::saveTextFile(this, "Export summary report", "report.txt", "Text files (*.txt)", summary(placed));
    }

    void exportPdf(int placed) {
        const QString path = QFileDialog::getSaveFileName(this, "Export PDF report", QDir::homePath() + "/placement_report.pdf", "PDF files (*.pdf)");
        if (path.isEmpty()) return;
        QPdfWriter pdf(path);
        pdf.setPageSize(QPageSize(QPageSize::A4));
        pdf.setPageMargins(QMarginsF(18, 18, 18, 18), QPageLayout::Millimeter);
        QTextDocument doc;
        QFont f("Courier New", 10);
        doc.setDefaultFont(f);
        QString text = "PlaceBridge · Campus Placement Office\nCompany: All recruiting partners\n\n" +
                       summary(placed) + "\nCANDIDATES\n------------------------------------------\n";
        for (const Student &s : ctx.store->students())
            text += QString("%1  |  %2  |  CGPA %3  |  %4\n").arg(s.getName(), -20).arg(s.branch(), -24).arg(s.getCgpa(), -4).arg(s.getStatus());
        doc.setPlainText(text);
        doc.print(&pdf);
        QMessageBox::information(this, "Export complete", "Saved to:\n" + path);
    }

    void exportXlsx() {
        const QString path = QFileDialog::getSaveFileName(this, "Export Excel workbook",
            QDir::homePath() + "/placement_report.xlsx", "Excel workbook (*.xlsx)");
        if (path.isEmpty()) return;
        const auto students = ctx.store->students();
        const auto applications = ctx.store->applications();
        auto xmlEscape = [](QString value) {
            value.replace('&', "&amp;"); value.replace('<', "&lt;");
            value.replace('>', "&gt;"); value.replace('"', "&quot;");
            return value;
        };
        auto columnName = [](int index) {
            QString name;
            while (index > 0) {
                const int remainder = (index - 1) % 26;
                name.prepend(QChar('A' + remainder));
                index = (index - 1) / 26;
            }
            return name;
        };
        QString sheet = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><sheetData>";
        auto addRow = [&sheet, &xmlEscape, &columnName](int row, const QStringList &values) {
            sheet += QString("<row r=\"%1\">").arg(row);
            for (int col = 0; col < values.size(); ++col) {
                const QString ref = columnName(col + 1) + QString::number(row);
                sheet += QString("<c r=\"%1\" t=\"inlineStr\"><is><t xml:space=\"preserve\">%2</t>"
                                 "</is></c>").arg(ref, xmlEscape(values[col]));
            }
            sheet += "</row>";
        };
        addRow(1, { "Name", "Email", "Branch", "CGPA", "Status", "Applications", "Offers" });
        for (int i = 0; i < students.size(); ++i) {
            const Student &student = students[i];
            int count = 0, offers = 0;
            for (const Application &application : applications)
                if (application.student == student.getName()) {
                    ++count;
                    if (application.stage() >= 3) ++offers;   // match the CSV export
                }
            addRow(i + 2, { student.getName(), student.getEmail(), student.branch(),
                            student.getCgpa(), student.getStatus(), QString::number(count),
                            QString::number(offers) });
        }
        sheet += "</sheetData></worksheet>";

        QZipWriter zip(path);
        zip.addFile("[Content_Types].xml", QByteArray(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
            "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
            "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
            "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
            "<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
            "</Types>"));
        zip.addFile("_rels/.rels", QByteArray(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
            "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>"
            "</Relationships>"));
        zip.addFile("xl/workbook.xml", QByteArray(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
            "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
            "<sheets><sheet name=\"Placement Report\" sheetId=\"1\" r:id=\"rId1\"/></sheets></workbook>"));
        zip.addFile("xl/_rels/workbook.xml.rels", QByteArray(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
            "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>"
            "</Relationships>"));
        zip.addFile("xl/worksheets/sheet1.xml", sheet.toUtf8());
        zip.close();
        if (zip.status() != QZipWriter::NoError) {
            QMessageBox::critical(this, "Export failed", "Could not write the .xlsx workbook.");
            return;
        }
        QMessageBox::information(this, "Export complete", "Saved to:\n" + path);
    }

    void printPreview() {
        QPrinter printer(QPrinter::HighResolution);
        QPrintPreviewDialog preview(&printer, this);
        connect(&preview, &QPrintPreviewDialog::paintRequested, this, [this](QPrinter *target) {
            QTextDocument document;
            QString html = "<h1>PlaceBridge · Campus Placement Office</h1><p>Company: All recruiting partners</p>"
                           "<h2>Placement summary</h2><table border='1' cellspacing='0' cellpadding='5'>"
                           "<tr><th>Name</th><th>Company</th><th>Role</th><th>Status</th></tr>";
            for (const Application &application : ctx.store->applications())
                html += "<tr><td>" + application.student.toHtmlEscaped() + "</td><td>" +
                    application.company.toHtmlEscaped() + "</td><td>" +
                    application.role.toHtmlEscaped() + "</td><td>" +
                    application.status.toHtmlEscaped() + "</td></tr>";
            html += "</table>";
            document.setHtml(html);
            document.print(target);
        });
        preview.exec();
    }
};

#endif // REPORTS_PAGE_H
