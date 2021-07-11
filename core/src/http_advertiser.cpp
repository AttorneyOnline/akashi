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
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
            qDebug().noquote() << "Succesfully advertised server.";
        }
        else {
            QJsonDocument json = QJsonDocument::fromJson(reply->readAll());
            if (json.isNull()) {
                qCritical().noquote() << "Invalid JSON response from" << reply->url();
                reply->deleteLater();
                return;
            }

            qDebug().noquote() << "Got valid response from" << reply->url();
            qDebug() << json;
        }
    }
    reply->deleteLater();
}

void HTTPAdvertiser::setAdvertiserSettings(advertiser_config config)
{
    m_name = config.name;
    m_description = config.description;
    m_port = config.port;
    m_ws_port = config.ws_port;
    m_players = config.players;
    m_masterserver = config.masterserver;
    m_debug = config.debug;

    msAdvertiseServer();
}


