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

void Discord::postModcallWebhook(QString name, QString reason, int current_area)
{
    if (!QUrl (server->webhook_url).isValid()) {
        qWarning() << "Invalid webhook url!";
        return;
    }

    QNetworkRequest request(QUrl (server->webhook_url));
    QNetworkAccessManager* nam = new QNetworkAccessManager();
    connect(nam, &QNetworkAccessManager::finished,
            this, &Discord::onFinish);

    // This is the kind of garbage Qt makes me write.
    // I am so tired. Qt has broken me.
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject json;
    QJsonArray jsonArray;
    QJsonObject jsonObject {
        {"color", "13312842"},
        {"title", name + " filed a modcall in " + server->areas[current_area]->name},
        {"description", reason}
    };
    jsonArray.append(jsonObject);
    json["embeds"] = jsonArray;

    nam->post(request, QJsonDocument(json).toJson());

    if (server->webhook_sendfile) {
        QHttpMultiPart* construct = new QHttpMultiPart();
        request.setHeader(QNetworkRequest::ContentTypeHeader, "multipart/form-data; boundary=" + construct->boundary());

        //This cost me two days of my life. Thanks Qt and Discord. You have broken me.
        QHttpPart file;
        file.setRawHeader(QByteArray("Content-Disposition"), QByteArray("form-data; name=\"file\"; filename=\"log.txt\""));
        file.setRawHeader(QByteArray("Content-Type"), QByteArray("plain/text"));
        QQueue<QString> buffer = server->areas[current_area]->logger->getBuffer(); // I feel no shame for doing this
        QString log;
        while (!buffer.isEmpty()) {
            log.append(buffer.dequeue() + "\n");
        }
        file.setBody(log.toUtf8());
        construct->append(file);

        nam->post(request, construct);
    }
}

void Discord::onFinish(QNetworkReply *reply)
{
    QByteArray data = reply->readAll();
    QString str_reply = data;
    qDebug() << str_reply;
}
