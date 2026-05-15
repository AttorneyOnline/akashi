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

#include "core/server_settings.h"

#include "qnamespace.h"

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
        qWarning() << "Failed to advertise server. Serverlist URL is not valid. URL:" << serverlist.toString();
    }
}

void ServerPublisher::finished(QNetworkReply *f_reply)
{
    QNetworkReply *reply(f_reply);
    reply->deleteLater();
    QString remote_url = reply->url().toString();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Unable to connect to serverlist due to the following error:" << reply->errorString();
        qWarning() << "Remote URL:" << remote_url;
        return;
    }

    QByteArray data = reply->isReadable() ? reply->readAll() : QByteArray();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status != HTTP_OK) {
        QJsonParseError error;
        QJsonDocument document = QJsonDocument::fromJson(data, &error);

        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            qWarning() << "Received malformed response from masterserver. Error:" << error.errorString();
            qWarning() << "HTTP status code:" << status;
            qWarning() << "Parse error offset:" << error.offset;
            qWarning() << "Response body size:" << data.size() << "bytes";
            qWarning().noquote() << "Raw response body:" << QString::fromUtf8(data);
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
