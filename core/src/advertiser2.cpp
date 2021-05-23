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
#include "include/advertiser2.h"

Advertiser2::Advertiser2()
{
    m_manager = new QNetworkAccessManager();
    connect(m_manager, &QNetworkAccessManager::finished,
            this, &Advertiser2::readReply);
}

void Advertiser2::advertiseServer()
{
    if (m_masterserver.isValid()) {

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

        if (m_debug)
            qDebug() << "Advertised Server";
        return;
    }
    if (m_debug)
        qWarning() << "Unable to advertise. Masterserver URL '" + m_masterserver.toString() + "' is not valid.";
    return;

}

void Advertiser2::readReply(QNetworkReply *reply)
{
    if (m_debug) {
        QByteArray data = reply->readAll();
        qDebug() << data;
    }

}

void Advertiser2::setAdvertiserSettings(QString f_name, QString f_description, int f_port, int f_ws_port, int f_players, QUrl f_master_url, bool f_debug)
{
    m_name = f_name;
    m_description = f_description;
    m_port = f_port;
    m_ws_port = f_ws_port;
    m_players = f_players;
    m_masterserver = f_master_url;
    m_debug = f_debug;
}

void Advertiser2::updatePlayers(int f_players)
{
    m_players = f_players;
}
