#include "core/server_publisher.h"

#include "akashi/logging_categories.h"
#include "core/server_settings.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

const int HTTP_OK = 200;
const int WS_REVERSE_PROXY = 80;
const int TIMEOUT = 1000 * 60 * 4;

ServerPublisher::ServerPublisher(int port, int *player_count, ServerSettings *f_settings, QObject *parent) :
    QObject(parent),
    m_manager{new QNetworkAccessManager(this)},
    timeout_timer(new QTimer(this)),
    m_players(player_count),
    m_port{port},
    m_settings(f_settings)
{
    connect(m_manager, &QNetworkAccessManager::finished, this, &ServerPublisher::finished);
    connect(timeout_timer, &QTimer::timeout, this, &ServerPublisher::publishServer);

    timeout_timer->setTimerType(Qt::PreciseTimer);
    timeout_timer->setInterval(TIMEOUT);
    timeout_timer->start();
    publishServer();
}

void ServerPublisher::publishServer()
{
    if (!m_settings->advertise()) {
        return;
    }

    QUrl serverlist(m_settings->ms_ip());
    if (serverlist.isValid()) {
        QNetworkRequest request(serverlist);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QJsonObject serverinfo;
        if (!m_settings->hostname().trimmed().isEmpty()) {
            serverinfo["ip"] = m_settings->hostname();
        }
        if (m_settings->secure_port() != -1) {
            serverinfo["wss_port"] = m_settings->secure_port();
        }
        serverinfo["port"] = 27106;
        serverinfo["ws_port"] = m_settings->cloudflare_enabled() ? WS_REVERSE_PROXY : m_port;
        serverinfo["players"] = *m_players;
        serverinfo["name"] = m_settings->server_name();
        serverinfo["description"] = m_settings->server_description();

        m_manager->post(request, QJsonDocument(serverinfo).toJson());
    }
    else {
        qCWarning(akashiNet) << "Failed to advertise server. Serverlist URL is not valid. URL:" << serverlist.toString();
    }
}

void ServerPublisher::finished(QNetworkReply *f_reply)
{
    QNetworkReply *reply(f_reply);
    reply->deleteLater();
    QString remote_url = reply->url().toString();

    if (reply->error() != QNetworkReply::NoError) {
        qCWarning(akashiNet) << "Unable to connect to serverlist due to the following error:" << reply->errorString();
        qCWarning(akashiNet) << "Remote URL:" << remote_url;
        return;
    }

    QByteArray data = reply->isReadable() ? reply->readAll() : QByteArray();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status != HTTP_OK) {
        QJsonParseError error;
        QJsonDocument document = QJsonDocument::fromJson(data, &error);

        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            qCWarning(akashiNet) << "Received malformed response from masterserver. Error:" << error.errorString();
            qCWarning(akashiNet) << "HTTP status code:" << status;
            qCWarning(akashiNet) << "Parse error offset:" << error.offset;
            qCWarning(akashiNet) << "Response body size:" << data.size() << "bytes";
            qCWarning(akashiNet).noquote() << "Raw response body:" << QString::fromUtf8(data);
            return;
        }

        QJsonObject body = document.object();
        if (body.contains("errors")) {
            qCWarning(akashiNet) << "Failed to advertise to the serverlist due to the following errors:";
            const QJsonArray errors = body["errors"].toArray();
            for (const auto &ref : errors) {
                QJsonObject error = ref.toObject();
                qCWarning(akashiNet).noquote() << "Error:" << error["type"].toString() << ". Message:" << error["message"].toString();
            }
            return;
        }
    }
    qCInfo(akashiNet) << "Sucessfully advertised server to serverlist.";
}
