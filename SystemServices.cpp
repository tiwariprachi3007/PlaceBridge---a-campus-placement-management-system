#include "SystemServices.h"

#include "DataManager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>

QMutex &AppLogger::mutex() {
    static QMutex value;
    return value;
}

QString AppLogger::path() {
    // Resolved once: the location never changes while the process runs, and this
    // used to run an mkpath() on every single log line.
    static const QString value = [] {
        QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (dir.isEmpty()) dir = QDir::currentPath();
        QDir().mkpath(dir);
        return dir + "/placebridge.log";
    }();
    return value;
}

void AppLogger::write(const QString &level, const QString &message) {
    QMutexLocker lock(&mutex());
    const QString filePath = path();

    // Keep one rotated generation, then append. The previous implementation read
    // the whole log back and rewrote it for every line, so logging cost grew with
    // the log's size; appending keeps each write O(1).
    static qint64 writtenSinceCheck = 0;
    constexpr qint64 maximumSize = 1024 * 1024;
    QByteArray line = QString("%1 [%2] %3\n")
        .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs), level, message)
        .toUtf8();
    writtenSinceCheck += line.size();
    if (writtenSinceCheck >= 64 * 1024) {
        writtenSinceCheck = 0;
        const QFileInfo info(filePath);
        if (info.exists() && info.size() >= maximumSize) {
            const QString rotated = filePath + ".1";
            QFile::remove(rotated);
            QFile::rename(filePath, rotated);
        }
    }

    QFile output(filePath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Append)) return;
    output.write(line);
}

bool GlobalSearch::matches(const QStringList &terms, const QString &haystack) {
    for (const QString &term : terms)
        if (!haystack.contains(term, Qt::CaseInsensitive)) return false;
    return true;
}

QVector<SearchHit> GlobalSearch::run(const DataStore &store, const QString &query, int limit) {
    QVector<SearchHit> hits;
    const QStringList terms = query.simplified().split(' ', Qt::SkipEmptyParts);
    if (terms.isEmpty() || limit <= 0) return hits;
    hits.reserve(limit);

    for (const Student &student : store.students()) {
        if (hits.size() >= limit) break;
        if (!matches(terms, student.getName() + " " + student.getEmail() + " " + student.getCourse() + " " +
                             student.getRollNo() + " " + student.getSkills())) continue;
        hits.append({ "Student", student.getName(),
                      student.getEmail() + " · " + student.getCourse(), "Students" });
    }
    for (const Company &drive : store.drives()) {
        if (hits.size() >= limit) break;
        if (!matches(terms, drive.getName() + " " + drive.getJobRole() + " " + drive.getAllowedBranches()))
            continue;
        hits.append({ "Drive", drive.getName() + " — " + drive.getJobRole(),
                      drive.getPackageOffer() + " · " + drive.getDriveStatus(), "Drives" });
    }
    for (const Application &application : store.applications()) {
        if (hits.size() >= limit) break;
        if (!matches(terms, application.student + " " + application.company + " " + application.role + " " +
                             application.status)) continue;
        hits.append({ "Application", application.student + " → " + application.company,
                      application.role + " · " + application.status, "Applications" });
    }
    for (const Event &event : store.events()) {
        if (hits.size() >= limit) break;
        if (!matches(terms, event.title + " " + event.company + " " + event.detail)) continue;
        hits.append({ "Event", event.title, event.date.toString("dd MMM yyyy") + " " + event.time, "Calendar" });
    }
    return hits;
}

bool MockMailTransport::deliver(const QString &to, const QString &subject, const QString &body,
                                QString *savedPath, QString *error) {
    Q_UNUSED(to)
    Q_UNUSED(subject)
    Q_UNUSED(body)
    Q_UNUSED(savedPath)
    if (error) *error = "Mock transport does not send mail; use Save as .eml.";
    return false;
}

bool MockMailTransport::saveDraft(const QString &path, const QString &to, const QString &subject,
                                  const QString &body, QString *error) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    const QByteArray message = QString("To: %1\r\nSubject: %2\r\n"
        "MIME-Version: 1.0\r\nContent-Type: text/plain; charset=UTF-8\r\n\r\n%3\r\n")
        .arg(to, subject, body).toUtf8();
    if (file.write(message) != message.size() || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

RestClient::RestClient(QUrl endpoint, QObject *parent)
    : QObject(parent), baseUrl(std::move(endpoint)) {}

bool RestClient::isAuthenticated() const { return !token.isEmpty(); }

QNetworkReply *RestClient::login(const QString &email, const QString &password) {
    QNetworkRequest request(baseUrl.resolved(QUrl("/api/login")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    const QJsonObject body{ { "email", email }, { "password", password } };
    return manager.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
}

void RestClient::setToken(const QString &value) { token = value.toLatin1(); }

QNetworkReply *RestClient::get(const QString &path) {
    QNetworkRequest request(baseUrl.resolved(QUrl(path)));
    if (!token.isEmpty()) request.setRawHeader("Authorization", "Bearer " + token);
    return manager.get(request);
}

void RestClient::logout() {
    if (token.isEmpty()) return;
    QNetworkRequest request(baseUrl.resolved(QUrl("/api/logout")));
    request.setRawHeader("Authorization", "Bearer " + token);
    QNetworkReply *reply = manager.post(request, QByteArray());
    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    token.clear();
}

QString Util::initialsOf(const QString &text) {
    // Compiled once: this runs for every avatar tile in every list.
    static const QRegularExpression whitespace("\\s+");
    const QStringList words = text.trimmed().split(whitespace, Qt::SkipEmptyParts);
    if (words.isEmpty()) return "?";
    if (words.size() == 1) return words[0].left(2).toUpper();
    return (words[0].left(1) + words[1].left(1)).toUpper();
}

double Util::parseLpa(const QString &text) {
    // Compiled once: called for every drive card, sort comparison and report row.
    static const QRegularExpression number("([0-9]+(?:\\.[0-9]+)?)");
    const QRegularExpressionMatch match = number.match(text);
    return match.hasMatch() ? match.captured(1).toDouble() : 0.0;
}
