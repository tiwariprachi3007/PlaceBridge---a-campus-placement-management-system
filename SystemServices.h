#ifndef PLACEBRIDGE_SYSTEM_SERVICES_H
#define PLACEBRIDGE_SYSTEM_SERVICES_H

#include <QByteArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVector>

class QMutex;
class DataStore;

class AppLogger {
    static QMutex &mutex();
    static QString path();

public:
    static void write(const QString &level, const QString &message);
};

struct SearchHit {
    QString kind;
    QString title;
    QString detail;
    QString tab;
};

class GlobalSearch {
    static bool matches(const QStringList &terms, const QString &haystack);

public:
    static QVector<SearchHit> run(const DataStore &store, const QString &query, int limit = 40);
};

class MailTransport {
public:
    virtual ~MailTransport() = default;
    virtual bool deliver(const QString &to, const QString &subject, const QString &body,
                         QString *savedPath, QString *error) = 0;
};

class MockMailTransport final : public MailTransport {
public:
    bool deliver(const QString &to, const QString &subject, const QString &body,
                 QString *savedPath, QString *error) override;
    static bool saveDraft(const QString &path, const QString &to, const QString &subject,
                          const QString &body, QString *error = nullptr);
};

class RestClient final : public QObject {
    Q_OBJECT
    QNetworkAccessManager manager;
    QUrl baseUrl;
    QByteArray token;

public:
    explicit RestClient(QUrl endpoint, QObject *parent = nullptr);

    bool isAuthenticated() const;
    QNetworkReply *login(const QString &email, const QString &password);
    void setToken(const QString &value);
    QNetworkReply *get(const QString &path);
    void logout();
};

namespace Util {
QString initialsOf(const QString &text);
double parseLpa(const QString &text);
}

#endif // PLACEBRIDGE_SYSTEM_SERVICES_H
