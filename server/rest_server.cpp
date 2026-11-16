#include "DataManager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QHttpServer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QHash>
#include <QJsonValue>

class RestApi final : public QObject {
    QHttpServer server;
    QTcpServer listener;
    DataStore store;
    QHash<QString, QPair<QString, QDateTime>> tokens;

    QString authenticatedEmail(const QHttpServerRequest &request) {
        const QByteArray authorization = request.value("Authorization");
        if (!authorization.startsWith("Bearer ")) return {};
        const QString token = QString::fromLatin1(authorization.mid(7));
        auto it = tokens.find(token);
        if (it == tokens.end()) return {};
        if (it->second < QDateTime::currentDateTimeUtc()) {
            tokens.erase(it);
            return {};
        }
        return it->first;
    }

public:
    RestApi() {
        server.route("/api/health", []() {
            return QHttpServerResponse(QHttpServerResponse::StatusCode::Ok);
        });
        server.route("/api/login", QHttpServerRequest::Method::Post,
            [this](const QHttpServerRequest &request) {
                const QJsonObject body = QJsonDocument::fromJson(request.body()).object();
                const QString email = body.value("email").toString();
                const QString password = body.value("password").toString();
                store.setActor({});
                const auto account = store.authenticate(email, password);
                if (!account)
                    return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);
                const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
                tokens.insert(token, { account->email, QDateTime::currentDateTimeUtc().addSecs(3600) });
                QJsonObject response;
                response.insert("token", token);
                response.insert("expiresIn", 3600);
                response.insert("role", account->role);
                return QHttpServerResponse(response);
            });

        server.route("/api/me", QHttpServerRequest::Method::Get,
            [this](const QHttpServerRequest &request) {
                const QString email = authenticatedEmail(request);
                if (email.isEmpty())
                    return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);
                store.setActor(email);
                const auto account = store.findAccount(email);
                QJsonObject response;
                response.insert("email", email);
                if (account) {
                    response.insert("name", account->name);
                    response.insert("role", account->role);
                }
                return QHttpServerResponse(response);
            });

        server.route("/api/drives", QHttpServerRequest::Method::Get,
            [this](const QHttpServerRequest &request) {
                const QString email = authenticatedEmail(request);
                if (email.isEmpty())
                    return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);
                store.setActor(email);
                QJsonArray rows;
                for (const Company &drive : store.drives()) {
                    QJsonObject row;
                    row.insert("company", drive.getName());
                    row.insert("role", drive.getJobRole());
                    row.insert("package", drive.getPackageOffer());
                    row.insert("date", drive.getDriveDate());
                    row.insert("minimumCgpa", drive.getMinCgpa());
                    rows.append(row);
                }
                return QHttpServerResponse(rows);
            });

        server.route("/api/applications", QHttpServerRequest::Method::Get,
            [this](const QHttpServerRequest &request) {
                const QString email = authenticatedEmail(request);
                if (email.isEmpty())
                    return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);
                store.setActor(email);
                QJsonArray rows;
                for (const Application &application : store.applications()) {
                    QJsonObject row;
                    row.insert("student", application.student);
                    row.insert("company", application.company);
                    row.insert("role", application.role);
                    row.insert("status", application.status);
                    rows.append(row);
                }
                return QHttpServerResponse(rows);
            });

        server.route("/api/logout", QHttpServerRequest::Method::Post,
            [this](const QHttpServerRequest &request) {
                const QByteArray auth = request.value("Authorization");
                if (auth.startsWith("Bearer "))
                    tokens.remove(QString::fromLatin1(auth.mid(7)));
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Ok);
            });

    }

    bool start(quint16 port) {
        if (!listener.listen(QHostAddress::LocalHost, port)) return false;
        if (server.bind(&listener)) return true;
        listener.close();
        return false;
    }
};

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("PlaceBridge");
    QCoreApplication::setApplicationName("PlaceBridge");
    RestApi api;
    if (!api.start(8080)) return 1;
    return app.exec();
}
