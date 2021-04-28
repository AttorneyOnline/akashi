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
#ifndef DISCORD_H
#define DISCORD_H

#include <QtNetwork>
#include <QCoreApplication>
#include "area_data.h"

class Discord : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Creates an instance of the Discord class.
     *
     * @param p_server A pointer to the Server instance Discord is constructed by.
     * @param parent Qt-based parent, passed along to inherited constructor from QObject.
     */
    Discord(QObject* parent = nullptr)
        : QObject(parent) {
    };

    /**
     * @brief Whether discord webhooks are enabled on this server.
     */
    bool webhook_enabled;

    /**
     * @brief Requires link to be https and that both WebhookID and WebhookToken are present, if used for Discord.
     */
    QString webhook_url;

    /**
     * @brief If the modcall buffer is sent as a file.
     */
    bool webhook_sendfile;

public slots:

    /**
     * @brief Sends a modcall to a discord webhook.
     *
     * @param name The character or OOC name of the client who sent the modcall.
     * @param area The area name of the area the modcall was sent from.
     * @param reason The reason the client specified for the modcall.
     * @param current_area The index of the area the modcall is made.
     */
    void postModcallWebhook(QString name, QString reason, AreaData* area);

    /**
     * @brief Sends the reply to the POST request sent by Discord::postModcallWebhook.
     */
    void onFinish(QNetworkReply *reply);

};

#endif // DISCORD_H
