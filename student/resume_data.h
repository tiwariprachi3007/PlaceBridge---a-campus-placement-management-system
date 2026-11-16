#ifndef RESUME_DATA_H
#define RESUME_DATA_H

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QVector>

struct ResumeItem {
    QString title;
    QString subtitle;
    QString detail;
    QString year;
    QString score;
    QString description;
    QString technologies;
    QString link;

    QJsonObject toJson() const {
        return {
            { "title", title }, { "subtitle", subtitle }, { "detail", detail },
            { "year", year }, { "score", score }, { "description", description },
            { "technologies", technologies }, { "link", link }
        };
    }

    static ResumeItem fromJson(const QJsonObject &object) {
        ResumeItem item;
        item.title = object.value("title").toString();
        item.subtitle = object.value("subtitle").toString();
        item.detail = object.value("detail").toString();
        item.year = object.value("year").toString();
        item.score = object.value("score").toString();
        item.description = object.value("description").toString();
        item.technologies = object.value("technologies").toString();
        item.link = object.value("link").toString();
        return item;
    }
};

struct ResumeData {
    QString fullName;
    QString email;
    QString phone;
    QString location;
    QString linkedin;
    QString github;
    QString objective;
    QString technicalSkills;
    QString templateName = "Modern";
    QVector<ResumeItem> education;
    QVector<ResumeItem> projects;
    QVector<ResumeItem> experience;
    QVector<ResumeItem> certifications;
    QVector<ResumeItem> achievements;
    QVector<ResumeItem> activities;

    QJsonObject toJson() const {
        auto encodeItems = [](const QVector<ResumeItem> &items) {
            QJsonArray array;
            for (const ResumeItem &item : items) array.append(item.toJson());
            return array;
        };
        return {
            { "schemaVersion", 1 },
            { "fullName", fullName }, { "email", email }, { "phone", phone },
            { "location", location }, { "linkedin", linkedin }, { "github", github },
            { "objective", objective }, { "technicalSkills", technicalSkills },
            { "templateName", templateName },
            { "education", encodeItems(education) }, { "projects", encodeItems(projects) },
            { "experience", encodeItems(experience) },
            { "certifications", encodeItems(certifications) },
            { "achievements", encodeItems(achievements) },
            { "activities", encodeItems(activities) }
        };
    }

    static ResumeData fromJson(const QJsonObject &object) {
        ResumeData data;
        data.fullName = object.value("fullName").toString();
        data.email = object.value("email").toString();
        data.phone = object.value("phone").toString();
        data.location = object.value("location").toString();
        data.linkedin = object.value("linkedin").toString();
        data.github = object.value("github").toString();
        data.objective = object.value("objective").toString();
        data.technicalSkills = object.value("technicalSkills").toString();
        data.templateName = object.value("templateName").toString("Modern");
        auto decodeItems = [](const QJsonValue &value) {
            QVector<ResumeItem> items;
            if (!value.isArray()) return items;
            const QJsonArray array = value.toArray();
            items.reserve(array.size());
            for (const QJsonValue &entry : array)
                if (entry.isObject()) items.append(ResumeItem::fromJson(entry.toObject()));
            return items;
        };
        data.education = decodeItems(object.value("education"));
        data.projects = decodeItems(object.value("projects"));
        data.experience = decodeItems(object.value("experience"));
        data.certifications = decodeItems(object.value("certifications"));
        data.achievements = decodeItems(object.value("achievements"));
        data.activities = decodeItems(object.value("activities"));
        return data;
    }
};

class ResumeRepository {
    QString directory;

public:
    explicit ResumeRepository(const QString &storageDirectory = QString())
        : directory(storageDirectory) {
        if (directory.isEmpty()) {
            directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            if (directory.isEmpty()) directory = QDir::currentPath() + "/PlaceBridge";
            directory = QDir(directory).filePath("resumes");
        }
    }

    QString pathFor(const QString &email) const {
        const QString normalized = email.trimmed().toLower();
        if (normalized.isEmpty()) return {};
        const QByteArray digest = QCryptographicHash::hash(normalized.toUtf8(),
                                                            QCryptographicHash::Sha256).toHex();
        return QDir(directory).filePath(QString::fromLatin1(digest) + ".json");
    }

    QString load(const QString &email, ResumeData *result, bool *found = nullptr) const {
        if (!result) return "Resume data destination is unavailable.";
        const QString path = pathFor(email);
        if (path.isEmpty()) return "A student e-mail is required to load a resume.";
        if (found) *found = QFile::exists(path);
        if (!QFile::exists(path)) {
            *result = ResumeData();
            return {};
        }

        QFile input(path);
        if (!input.open(QIODevice::ReadOnly))
            return QString("Could not read the saved resume: %1").arg(input.errorString());
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(input.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
            return QString("The saved resume file is invalid: %1").arg(parseError.errorString());
        const QJsonObject object = document.object();
        if (object.value("schemaVersion").toInt(-1) != 1)
            return "This resume was saved in an unsupported format.";
        *result = ResumeData::fromJson(object);
        return {};
    }

    QString save(const QString &email, const ResumeData &data) const {
        const QString path = pathFor(email);
        if (path.isEmpty()) return "A student e-mail is required to save a resume.";
        if (!QDir().mkpath(directory))
            return QString("Could not create the resume storage folder: %1").arg(directory);

        QSaveFile output(path);
        if (!output.open(QIODevice::WriteOnly))
            return QString("Could not save the resume: %1").arg(output.errorString());
        const QByteArray bytes = QJsonDocument(data.toJson()).toJson(QJsonDocument::Indented);
        if (output.write(bytes) != bytes.size()) {
            const QString error = output.errorString();
            output.cancelWriting();
            return QString("Could not write the resume data: %1").arg(error);
        }
        if (!output.commit())
            return QString("Could not finalize the saved resume: %1").arg(output.errorString());
        return {};
    }
};

#endif // RESUME_DATA_H
