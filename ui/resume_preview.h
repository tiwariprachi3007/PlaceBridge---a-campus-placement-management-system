#ifndef RESUME_PREVIEW_H
#define RESUME_PREVIEW_H

#include "resume_data.h"

#include <algorithm>
#include <QFile>
#include <QFileInfo>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QSaveFile>
#include <QRegularExpression>
#include <QTextDocument>
#include <QUrl>

class ResumeDocumentRenderer {
public:
    static QString html(const ResumeData &resume) {
        const QString templateName = resume.templateName == "Classic" ? "classic"
                                   : resume.templateName == "Minimal" ? "minimal" : "modern";
        const QString templateStyle = templateName == "classic"
            ? "body{font-family:Georgia,serif;color:#232c2a}"
              "header{padding:0 0 11px;margin-bottom:17px;border-bottom:2px solid #103c35}"
              "h1{font-size:25pt;color:#103c35;letter-spacing:0.4px}"
              "h2{font-size:11.5pt;color:#103c35;border-bottom:1px solid #c3d2cd;letter-spacing:1.4px}"
              ".accent{color:#176b5b}.entry-title{font-weight:bold;font-size:11pt}"
              ".contact{font-size:9.5pt;color:#45544f}.when{font-style:italic}"
            : templateName == "minimal"
            ? "body{font-family:Arial,sans-serif;color:#2c3133}"
              "header{padding:0 0 9px;margin-bottom:15px;border-bottom:1px solid #cfd5d7}"
              "h1{font-size:21pt;color:#20262a;letter-spacing:-0.3px}"
              "h2{font-size:10pt;color:#5b676b;border-bottom:1px solid #e0e4e6;letter-spacing:1.6px}"
              ".accent{color:#5b676b}.entry-title{font-weight:bold;font-size:10.5pt}"
              ".contact{font-size:9pt;color:#5b676b}"
              "section{margin:0 0 13px}"
            : "body{font-family:Arial,sans-serif;color:#22302d}"
              "h1{font-size:30pt;color:#ffffff;letter-spacing:-0.6px}"
              "h2{font-size:11.5pt;color:#103c35;border-bottom:2px solid #e58a45;letter-spacing:1.3px}"
              ".accent{color:#e58a45}.entry-title{font-weight:bold;font-size:11pt}"
              ".contact{font-size:9.5pt}";

        QString output = QString(
            "<html><head><meta charset=\"utf-8\"><style>"
            "body{margin:0;font-size:10pt;line-height:1.4;color:#283633}"
            "header{padding:0 0 14px;margin-bottom:16px}"
            "h1{margin:0 0 6px;font-weight:700;line-height:1.1}"
            ".contact{font-size:9pt;line-height:1.5;overflow-wrap:anywhere;word-wrap:break-word}"
            "section{margin:0 0 14px}"
            "h2{margin:0 0 8px;padding:0 0 3px;text-transform:uppercase}"
            ".entry{margin:0 0 9px;page-break-inside:avoid;overflow-wrap:anywhere;word-wrap:break-word}"
            ".entry-title{line-height:1.22}"
            ".when{font-size:9pt;color:#53645f;white-space:nowrap}"
            ".meta{font-size:9pt;color:#53645f;margin:1px 0 3px;line-height:1.32}"
            ".description{white-space:pre-wrap;margin:2px 0 0;line-height:1.42}"
            ".skills{font-size:9.5pt;line-height:1.6}"
            ".empty{color:#74837e;font-style:italic;margin-top:14px}"
            "a{color:#176b5b;text-decoration:none}"
            "%1</style></head><body class=\"%2\">")
            .arg(templateStyle, templateName);

        const QString name = resume.fullName.trimmed().isEmpty()
            ? QString("Your Name") : escape(resume.fullName.trimmed());
        QStringList contacts;
        const QStringList contactValues = { resume.email, resume.phone, resume.location };
        for (const QString &value : contactValues)
            if (!value.trimmed().isEmpty()) contacts.append(escape(value.trimmed()));
        if (!resume.linkedin.trimmed().isEmpty()) contacts.append(link(resume.linkedin));
        if (!resume.github.trimmed().isEmpty()) contacts.append(link(resume.github));

        const QString contactLine = contacts.join(" &nbsp;|&nbsp; ");
        if (templateName == "modern") {
            output += "<table width=\"100%\" cellpadding=\"18\" cellspacing=\"0\" bgcolor=\"#103c35\">"
                      "<tr><td align=\"center\"><h1>" + name + "</h1>";
            if (!contacts.isEmpty())
                output += "<div class=\"contact\" style=\"color:#ffffff\">" + contactLine + "</div>";
            output += "</td></tr></table>";
        } else {
            output += "<header><h1>" + name + "</h1>";
            if (!contacts.isEmpty()) output += "<div class=\"contact\">" + contactLine + "</div>";
            output += "</header>";
        }

        bool hasContent = false;
        hasContent |= addTextSection(output, "Professional Summary", resume.objective);
        hasContent |= addEducation(output, resume.education);
        hasContent |= addSkills(output, resume.technicalSkills);
        hasContent |= addProjects(output, resume.projects);
        hasContent |= addExperience(output, resume.experience);
        hasContent |= addSimpleItems(output, "Certifications", resume.certifications);
        hasContent |= addSimpleItems(output, "Achievements", resume.achievements);
        hasContent |= addSimpleItems(output, "Extracurricular Activities", resume.activities);
        if (!hasContent) output += "<p class=\"empty\">Add resume details in the editor to build your preview.</p>";
        output += "</body></html>";
        return output;
    }

private:
    static QString escape(const QString &value) {
        QString text = value.toHtmlEscaped();
        text.replace("\r\n", "\n");
        text.replace('\r', '\n');
        text.replace('\n', "<br>");
        return text;
    }

    static QString link(const QString &value) {
        const QString label = escape(value.trimmed());
        const QUrl url = QUrl::fromUserInput(value.trimmed());
        if (url.isValid() && (url.scheme() == "http" || url.scheme() == "https"))
            return QString("<a href=\"%1\">%2</a>")
                .arg(url.toString(QUrl::FullyEncoded).toHtmlEscaped(), label);
        return label;
    }

    static QString meta(const QStringList &values) {
        QStringList parts;
        for (const QString &value : values)
            if (!value.trimmed().isEmpty()) parts.append(escape(value.trimmed()));
        return parts.join(" &nbsp;|&nbsp; ");
    }

    static bool addTextSection(QString &output, const QString &title, const QString &text) {
        if (text.trimmed().isEmpty()) return false;
        output += "<section><h2>" + escape(title) + "</h2><div class=\"description\">" +
                  escape(text.trimmed()) + "</div></section>";
        return true;
    }

    // Title on the left, date on the right, on one baseline. A plain <div> stack
    // put the year on its own line, which is not how a resume reads.
    static QString titleRow(const QString &title, const QString &when) {
        const QString left = title.trimmed().isEmpty() ? QString() : escape(title.trimmed());
        const QString right = when.trimmed().isEmpty() ? QString() : escape(when.trimmed());
        if (right.isEmpty())
            return left.isEmpty() ? QString()
                                  : "<div class=\"entry-title\">" + left + "</div>";
        return "<table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\"><tr>"
               "<td class=\"entry-title\" align=\"left\">" + left + "</td>"
               "<td class=\"when\" align=\"right\">" + right + "</td></tr></table>";
    }

    static bool addSkills(QString &output, const QString &text) {
        if (text.trimmed().isEmpty()) return false;
        // Comma-separated input renders as a single spaced run instead of a
        // paragraph, so a long skill list stays scannable.
        QStringList skills;
        const QStringList parts = text.split(QRegularExpression("[,\n;]"), Qt::SkipEmptyParts);
        for (const QString &part : parts)
            if (!part.trimmed().isEmpty()) skills.append(escape(part.trimmed()));
        output += "<section><h2>Technical Skills</h2><div class=\"skills\">";
        output += skills.isEmpty() ? escape(text.trimmed())
                                   : skills.join(" &nbsp;&middot;&nbsp; ");
        output += "</div></section>";
        return true;
    }

    static bool addEducation(QString &output, const QVector<ResumeItem> &items) {
        QString body;
        for (const ResumeItem &item : items) {
            if (item.title.trimmed().isEmpty() && item.subtitle.trimmed().isEmpty() &&
                item.detail.trimmed().isEmpty() && item.year.trimmed().isEmpty() &&
                item.score.trimmed().isEmpty()) continue;
            body += "<div class=\"entry\">";
            body += titleRow(item.title, item.year);
            const QString details = meta({ item.subtitle, item.detail,
                item.score.trimmed().isEmpty() ? QString() : "CGPA " + item.score.trimmed() });
            if (!details.isEmpty()) body += "<div class=\"meta\">" + details + "</div>";
            body += "</div>";
        }
        if (body.isEmpty()) return false;
        output += "<section><h2>Education</h2>" + body + "</section>";
        return true;
    }

    static bool addProjects(QString &output, const QVector<ResumeItem> &items) {
        QString body;
        for (const ResumeItem &item : items) {
            if (item.title.trimmed().isEmpty() && item.description.trimmed().isEmpty() &&
                item.technologies.trimmed().isEmpty() && item.link.trimmed().isEmpty()) continue;
            body += "<div class=\"entry\">";
            if (!item.title.trimmed().isEmpty())
                body += "<div class=\"entry-title\">" + escape(item.title.trimmed()) + "</div>";
            QStringList details;
            if (!item.technologies.trimmed().isEmpty()) details.append("Technologies: " + item.technologies);
            if (!item.link.trimmed().isEmpty()) details.append("Project: " + item.link);
            const QString metadata = meta(details);
            if (!metadata.isEmpty()) body += "<div class=\"meta\">" + metadata + "</div>";
            if (!item.description.trimmed().isEmpty())
                body += "<div class=\"description\">" + escape(item.description.trimmed()) + "</div>";
            body += "</div>";
        }
        if (body.isEmpty()) return false;
        output += "<section><h2>Projects</h2>" + body + "</section>";
        return true;
    }

    static bool addExperience(QString &output, const QVector<ResumeItem> &items) {
        QString body;
        for (const ResumeItem &item : items) {
            if (item.title.trimmed().isEmpty() && item.subtitle.trimmed().isEmpty() &&
                item.detail.trimmed().isEmpty() && item.description.trimmed().isEmpty()) continue;
            body += "<div class=\"entry\">";
            body += titleRow(item.title, item.detail);
            const QString details = meta({ item.subtitle });
            if (!details.isEmpty()) body += "<div class=\"meta\">" + details + "</div>";
            if (!item.description.trimmed().isEmpty())
                body += "<div class=\"description\">" + escape(item.description.trimmed()) + "</div>";
            body += "</div>";
        }
        if (body.isEmpty()) return false;
        output += "<section><h2>Experience</h2>" + body + "</section>";
        return true;
    }

    static bool addSimpleItems(QString &output, const QString &heading,
                               const QVector<ResumeItem> &items) {
        QString body;
        for (const ResumeItem &item : items) {
            if (item.title.trimmed().isEmpty() && item.description.trimmed().isEmpty()) continue;
            body += "<div class=\"entry\">";
            if (!item.title.trimmed().isEmpty())
                body += "<div class=\"entry-title\">" + escape(item.title.trimmed()) + "</div>";
            if (!item.description.trimmed().isEmpty())
                body += "<div class=\"description\">" + escape(item.description.trimmed()) + "</div>";
            body += "</div>";
        }
        if (body.isEmpty()) return false;
        output += "<section><h2>" + escape(heading) + "</h2>" + body + "</section>";
        return true;
    }
};

// Tells the student what a recruiter would find missing, rather than only
// showing them what they have already typed.
struct ResumeScore {
    int percent = 0;
    QStringList missing;
};

class ResumeScorer {
public:
    static ResumeScore evaluate(const ResumeData &resume) {
        struct Check { bool passed; int weight; const char *advice; };
        const QStringList skills = resume.technicalSkills.split(
            QRegularExpression("[,\n;]"), Qt::SkipEmptyParts);
        const bool hasDescribedProject = std::any_of(
            resume.projects.cbegin(), resume.projects.cend(), [](const ResumeItem &item) {
                return !item.title.trimmed().isEmpty() && !item.description.trimmed().isEmpty();
            });

        const QVector<Check> checks = {
            { !resume.fullName.trimmed().isEmpty(), 10, "Add your full name" },
            { !resume.email.trimmed().isEmpty(), 10, "Add a contact e-mail" },
            { !resume.phone.trimmed().isEmpty(), 5, "Add a phone number" },
            { !resume.location.trimmed().isEmpty(), 5, "Add your city" },
            { !resume.linkedin.trimmed().isEmpty() || !resume.github.trimmed().isEmpty(), 10,
              "Add a LinkedIn or GitHub link" },
            { resume.objective.trimmed().size() >= 40, 10,
              "Write a summary of at least a couple of lines" },
            { skills.size() >= 4, 15, "List at least four technical skills" },
            { !resume.education.isEmpty(), 10, "Add your degree under Education" },
            { hasDescribedProject, 15, "Add a project with a description of what you built" },
            { !resume.experience.isEmpty() || !resume.certifications.isEmpty(), 5,
              "Add an internship or a certification" },
            { !resume.achievements.isEmpty() || !resume.activities.isEmpty(), 5,
              "Add an achievement or an activity" }
        };

        ResumeScore score;
        int earned = 0, total = 0;
        for (const Check &check : checks) {
            total += check.weight;
            if (check.passed) earned += check.weight;
            else score.missing.append(QString::fromLatin1(check.advice));
        }
        score.percent = total == 0 ? 0 : qRound(100.0 * earned / total);
        return score;
    }
};

class ResumePdfExporter {
public:
    static QString exportFile(const QString &path, const ResumeData &resume) {
        if (path.trimmed().isEmpty()) return "Choose a PDF file path.";
        QSaveFile output(path);
        if (!output.open(QIODevice::WriteOnly))
            return QString("Could not create the PDF: %1").arg(output.errorString());
        {
            QPdfWriter pdf(&output);
            pdf.setPageSize(QPageSize(QPageSize::A4));
            pdf.setPageMargins(QMarginsF(16, 16, 16, 16), QPageLayout::Millimeter);
            QTextDocument document;
            document.setDocumentMargin(0);
            document.setDefaultFont(QFont("Arial", 10));
            document.setHtml(ResumeDocumentRenderer::html(resume));
            document.print(&pdf);
        }
        if (!output.commit())
            return QString("Could not finish writing the PDF: %1").arg(output.errorString());

        QFile check(path);
        if (!check.open(QIODevice::ReadOnly))
            return QString("The PDF was written but could not be verified: %1").arg(check.errorString());
        if (!check.read(5).startsWith("%PDF-"))
            return "The exported file is not a valid PDF.";
        return {};
    }
};

#endif // RESUME_PREVIEW_H
