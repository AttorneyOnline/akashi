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
#include "publisher.h"
#include "qjsonarray.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QnetworkReply>

const int HTTP_OK = 200;
const int REVERSE_PROXY_PORT = 80;

Publisher::Publisher(akashi::PublisherInfo *config, QObject *parent) :
    QObject{parent},
    m_config{config},
    m_serverlist{new QNetworkAccessManager(this)}
{
    timeout_timer.setTimerType(Qt::PreciseTimer);
    timeout_timer.setInterval(TIMEOUT_TIME);
    timeout_timer.setSingleShot(false);
    connect(&timeout_timer, &QTimer::timeout, this, &Publisher::publishServerInfo);
    connect(m_serverlist, &QNetworkAccessManager::finished, this, &Publisher::finished);

    timeout_timer.start();
    publishServerInfo();
}

void Publisher::publishServerInfo()
{
    if (!m_config->enabled) {
        return; //< Publishing is disabled. The server will try again on next timeout.
    }

    QUrl serverlist(m_config->serverlist);
    if (serverlist.isValid()) {
        QNetworkRequest request(serverlist);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QJsonObject serverinfo;
        if (m_config->ip.has_value()) {
            serverinfo["ip"] = m_config->ip.value();
        }
        serverinfo["port"] = 27106;
        serverinfo["ws_port"] = m_config->rp_enabled ? m_config->port : REVERSE_PROXY_PORT;
        serverinfo["players"] = m_config->players;
        serverinfo["name"] = m_config->servername;
        serverinfo["description"] = m_config->description;

        m_serverlist->post(request, QJsonDocument(serverinfo).toJson());
    }
    else {
        qWarning() << "Failed to advertise server. Serverlist URL is not valid. URL:" << serverlist.toString();
    }
}

void Publisher::finished(QNetworkReply *f_reply)
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
            for (auto ref : errors) {
                QJsonObject error = ref.toObject();
                qWarning().noquote() << "Error:" << error["type"].toString()
                                     << "- The key" << error["path"].toString() << error["message"].toString();
            }
            return;
        }
    }
    qInfo() << "Sucessfully advertised server to serverlist.";
}
