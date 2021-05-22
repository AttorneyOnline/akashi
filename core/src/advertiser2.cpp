#include "include/advertiser2.h"

Advertiser2::Advertiser2()
{
    m_manager = new QNetworkAccessManager();
    connect(m_manager, &QNetworkAccessManager::finished,
            this, &Advertiser2::readReply);
}

void Advertiser2::advertiseServer()
{
    if (m_masterserver.isValid()){
        qWarning() << "Unable to advertise. Masterserver URL '" + m_masterserver.toString() + "' is not valid.";
        return;
    }

    QNetworkRequest request((QUrl (m_masterserver)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject json;
    json["port"] = m_port;
    if (!(m_ws_port == -1)) {
        json["ws_port"] = m_ws_port;
    }

    json["players"] = m_players;
    json["name"] = m_name;

    if (!(m_description.isEmpty())) {
    json["description"] = m_description;
    }

    m_manager->post(request, QJsonDocument(json).toJson());
}

void Advertiser2::readReply(QNetworkReply *reply)
{
    QByteArray data = reply->readAll();
    qDebug() << data;
}

void Advertiser2::setAdvertiserSettings(QString f_name, QString f_description, int f_port, int f_ws_port, int f_players, QUrl f_master_url)
{
    m_name = f_name;
    m_description = f_description;
    m_port = f_port;
    m_ws_port = f_ws_port;
    m_players = f_players;
    m_masterserver = f_master_url;
}

void Advertiser2::updatePlayers(int f_players)
{
    m_players = f_players;
}
