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
#include "include/discord.h"

void Discord::postModcallWebhook(QString name, QString area, QString reason)
{
    if (!QUrl (server->webhook_url).isValid()) {
        qWarning() << "Invalid webhook url!";
        return;
    }
    QNetworkRequest request(QUrl (server->webhook_url));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // This is the kind of garbage Qt makes me write.
    // I am so tired. Qt has broken me.
    QJsonObject json;
    QJsonArray jsonArray;
    QJsonObject jsonObject {
        {"color", "13312842"},
        {"title", name + " filed a modcall in " + area},
        {"description", reason}
    };

    jsonArray.append(jsonObject);
    json["embeds"] = jsonArray;

    QNetworkAccessManager* nam = new QNetworkAccessManager();
    connect(nam, &QNetworkAccessManager::finished,
            this, &Discord::onFinish);

    nam->post(request, QJsonDocument(json).toJson());
}

void Discord::onFinish(QNetworkReply *reply)
{
    QByteArray data = reply->readAll();
    QString str_reply = data;
    qDebug() << str_reply;
}
