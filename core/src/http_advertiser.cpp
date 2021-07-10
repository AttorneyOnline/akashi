#include "include/http_advertiser.h"

HTTPAdvertiser::HTTPAdvertiser()
{
    m_manager = new QNetworkAccessManager();
    connect(m_manager, &QNetworkAccessManager::finished,
            this, &HTTPAdvertiser::msRequestFinished);
}

HTTPAdvertiser::~HTTPAdvertiser()
{
    m_manager->deleteLater();
}

void HTTPAdvertiser::msAdvertiseServer()
{
    if (m_masterserver.isValid()) {

        QUrl url(m_masterserver);
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QJsonObject json;
        json["port"] = m_port;
        if (m_ws_port != -1) {
            json["ws_port"] = m_ws_port;
        }

        json["players"] = m_players;
        json["name"] = m_name;

        if (!m_description.isEmpty()) {
        json["description"] = m_description;
        }

        m_manager->post(request, QJsonDocument(json).toJson());

        if (m_debug)
            qDebug().noquote() << "Advertised Server";
        return;
    }
    if (m_debug)
        qWarning().noquote() << "Unable to advertise. Masterserver URL '" + m_masterserver.toString() + "' is not valid.";
    return;

}

void HTTPAdvertiser::msRequestFinished(QNetworkReply *reply)
{
    if (m_debug) {
        QJsonDocument json = QJsonDocument::fromJson(reply->readAll());
        if (json.isNull()) {
            qCritical().noquote() << "Invalid JSON response from" << reply->url();
            reply->deleteLater();
            return;
        }

        qDebug().noquote() << "Got valid response from" << reply->url();
        qDebug() << json;
    }
    reply->deleteLater();
}

void HTTPAdvertiser::setAdvertiserSettings(QString f_name, QString f_description, int f_port, int f_ws_port, int f_players, QUrl f_master_url, bool f_debug)
{
    m_name = f_name;
    m_description = f_description;
    m_port = f_port;
    m_ws_port = f_ws_port;
    m_players = f_players;
    m_masterserver = f_master_url;
    m_debug = f_debug;

    msAdvertiseServer();
}
