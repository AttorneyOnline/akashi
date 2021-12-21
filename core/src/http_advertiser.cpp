#include "include/http_advertiser.h"

HTTPAdvertiser::HTTPAdvertiser()
{
    m_manager = new QNetworkAccessManager();
    connect(m_manager, &QNetworkAccessManager::finished,
            this, &HTTPAdvertiser::msRequestFinished);


    m_name = ConfigManager::serverName();
    m_hostname = ConfigManager::advertiserHostname();
    m_description = ConfigManager::serverDescription();
    m_port = ConfigManager::serverPort();
    m_ws_port = ConfigManager::webaoPort();
    m_masterserver = ConfigManager::advertiserHTTPIP();
    m_debug = ConfigManager::advertiserHTTPDebug();
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

        QJsonObject l_json;

        if (!m_hostname.isEmpty()) {
            l_json["ip"] = m_hostname;
        }

        l_json["port"] = m_port;
        if (m_ws_port != -1) {
            l_json["ws_port"] = m_ws_port;
        }

        l_json["players"] = m_players;
        l_json["name"] = m_name;

        if (!m_description.isEmpty()) {
        l_json["description"] = m_description;
        }

        m_manager->post(request, QJsonDocument(l_json).toJson());

        if (m_debug)
            qDebug().noquote() << "Advertised Server";
        return;
    }
    if (m_debug)
        qWarning().noquote() << "Unable to advertise. Masterserver URL '" + m_masterserver.toString() + "' is not valid.";
    return;

}

void HTTPAdvertiser::msRequestFinished(QNetworkReply *f_reply)
{
    if (m_debug) {
        if (f_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
            qDebug().noquote() << "Succesfully advertised server.";
        }
        else {
            QJsonDocument json = QJsonDocument::fromJson(f_reply->readAll());
            if (json.isNull()) {
                qCritical().noquote() << "Invalid JSON response from" << f_reply->url();
                f_reply->deleteLater();
                return;
            }

            qDebug().noquote() << "Got valid response from" << f_reply->url();
            qDebug() << json;
        }
    }
    f_reply->deleteLater();
}

void HTTPAdvertiser::updatePlayerCount(int f_current_players)
{
    m_players = f_current_players;
}

void HTTPAdvertiser::updateAdvertiserSettings()
{
    m_name = ConfigManager::serverName();
    m_hostname = ConfigManager::advertiserHostname();
    m_description = ConfigManager::serverDescription();
    m_masterserver = ConfigManager::advertiserHTTPIP();
    m_debug = ConfigManager::advertiserHTTPDebug();
}


