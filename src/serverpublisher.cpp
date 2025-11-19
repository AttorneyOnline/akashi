//////////////////////////////////////////////////////////////////////////////////////
//    akashi - a server for Attorney Online 2                                       //
//    Copyright (C) 2020  scatterflower                                             //
//                                                                                  //
//    This program is free software: you can redistribute it and/or modify          //
//    it under the terms of the GNU Affero General Public License as                //
//    published by the Free Software Foundation, either version 3 of the            //
//    License, or (at your option) any later version.                               //
//                                                                                  //
//    This program is distributed in the hope that it will be useful,               //
//    but WITHOUT ANY WARRANTY; without even the implied warranty of                //
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 //
//    GNU Affero General Public License for more details.                           //
//                                                                                  //
//    You should have received a copy of the GNU Affero General Public License      //
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.        //
//////////////////////////////////////////////////////////////////////////////////////
#include "serverpublisher.h"
#include "config_manager.h"
#include "qnamespace.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

const int HTTP_OK = 200;
const int WS_REVERSE_PROXY = 80;
const int TIMEOUT = 1000 * 60 * 4;

ServerPublisher::ServerPublisher(int port, int *player_count, QObject *parent) :
    QObject(parent),
    m_manager{new QNetworkAccessManager(this)},
    timeout_timer(new QTimer(this)),
    m_players(player_count),
    m_port{port}
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
    if (!ConfigManager::publishServerEnabled()) {
        return;
    }

    QUrl serverlist(ConfigManager::serverlistURL());
    if (serverlist.isValid()) {
        QNetworkRequest request(serverlist);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QJsonObject serverinfo;
        if (!ConfigManager::serverDomainName().trimmed().isEmpty()) {
            serverinfo["ip"] = ConfigManager::serverDomainName();
        }
        if (ConfigManager::securePort() != -1) {
            serverinfo["wss_port"] = ConfigManager::securePort();
        }
        serverinfo["port"] = 27106;
        serverinfo["ws_port"] = ConfigManager::advertiseWSProxy() ? WS_REVERSE_PROXY : m_port;
        serverinfo["players"] = *m_players;
        serverinfo["name"] = ConfigManager::serverName();
        serverinfo["description"] = ConfigManager::serverDescription();

        m_manager->post(request, QJsonDocument(serverinfo).toJson());
    }
    else {
        qWarning() << "Failed to advertise server. Serverlist URL is not valid. URL:" << serverlist.toString();
    }
}

void ServerPublisher::finished(QNetworkReply *f_reply)
{
    QNetworkReply *reply(f_reply);
    QString remote_url = reply->url().toString();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Unable to connect to serverlist due to the following error:" << reply->errorString();
        qWarning() << "Remote URL:" << remote_url;
    }

    QByteArray data = reply->isReadable() ? reply->readAll() : QByteArray();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status != HTTP_OK) {
        QJsonParseError error;
        QJsonDocument document = QJsonDocument::fromJson(data, &error);

        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            qWarning() << "Received malformed response from masterserver. Error:" << error.errorString();
            return;
        }

        QJsonObject body = document.object();
        if (body.contains("errors")) {
            qWarning() << "Failed to advertise to the serverlist due to the following errors:";
            const QJsonArray errors = body["errors"].toArray();
            for (const auto &ref : errors) {
                QJsonObject error = ref.toObject();
                qWarning().noquote() << "Error:" << error["type"].toString() << ". Message:" << error["message"].toString();
            }
            return;
        }
    }
    qInfo() << "Sucessfully advertised server to serverlist.";
}
