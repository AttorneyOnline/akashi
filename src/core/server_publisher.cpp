#include "core/server_publisher.h"

#include "akashi/logging_categories.h"
#include "core/server_settings.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSslError>
#include <QTimer>

const int HTTP_OK = 200;
const int WS_REVERSE_PROXY = 80;
const int TIMEOUT = 1000 * 60 * 4;
// A stalled post must fail instead of lingering until the next publish.
const int TRANSFER_TIMEOUT = 1000 * 30;

ServerPublisher::ServerPublisher(int port, int *player_count, ServerSettings *f_settings, QObject *parent) :
    QObject(parent),
    m_manager{new QNetworkAccessManager(this)},
    timeout_timer(new QTimer(this)),
    m_players(player_count),
    m_port{port},
    m_settings(f_settings)
{
    m_manager->setTransferTimeout(TRANSFER_TIMEOUT);
    connect(m_manager, &QNetworkAccessManager::finished, this, &ServerPublisher::finished);
    connect(m_manager, &QNetworkAccessManager::sslErrors, this, &ServerPublisher::onSslErrors);
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
    f_reply->deleteLater();
    const QString remote_url = f_reply->url().toString();
    const QByteArray data = f_reply->isReadable() ? f_reply->readAll() : QByteArray();
    const int status = f_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (f_reply->error() == QNetworkReply::NoError && status == HTTP_OK) {
        qCInfo(akashiNet) << "Successfully advertised server to serverlist.";
        return;
    }

    qCWarning(akashiNet) << "Failed to advertise to the serverlist:" << f_reply->errorString();
    qCWarning(akashiNet) << "Remote URL:" << remote_url << "- HTTP status code:" << status;

    // An HTTP-level refusal carries the masterserver's error report in the
    // body; a transport failure has no body worth parsing.
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        if (!data.isEmpty()) {
            qCWarning(akashiNet).noquote() << "Raw response body:" << QString::fromUtf8(data);
        }
        return;
    }

    const QJsonArray errors = document.object()["errors"].toArray();
    for (const auto &ref : errors) {
        const QJsonObject entry = ref.toObject();
        qCWarning(akashiNet).noquote() << "Error:" << entry["type"].toString() << ". Message:" << entry["message"].toString();
    }
}

void ServerPublisher::onSslErrors(QNetworkReply *f_reply, const QList<QSslError> &f_errors)
{
    for (const QSslError &l_error : f_errors) {
        qCWarning(akashiNet) << "SSL error advertising to" << f_reply->url().toString() << "-" << l_error.errorString();
    }
}
