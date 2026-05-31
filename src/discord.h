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
#pragma once

#include <QCoreApplication>
#include <QtNetwork>

#include <functional>

class DiscordSettings;

namespace akashi {
class EventBus;
struct ModcallEvent;
struct BanIssuedEvent;
} // namespace akashi

class Discord : public QObject
{
    Q_OBJECT

  public:
    using AreaBufferFn = std::function<QString(const QString &)>;

    Discord(DiscordSettings *f_settings, akashi::EventBus *f_event_bus,
            AreaBufferFn f_area_buffer, QObject *parent = nullptr);
    ~Discord();

  private:
    void onModcall(akashi::ModcallEvent &f_event);
    void onBan(akashi::BanIssuedEvent &f_event);

    QJsonDocument constructModcallJson(const QString &f_name, const QString &f_area,
                                       const QString &f_id, const QString &f_reason) const;
    QJsonDocument constructBanJson(const QString &f_ipid, const QString &f_moderator,
                                   const QString &f_duration, const QString &f_reason, int f_ban_id);
    QHttpMultiPart *constructLogMultipart(const QString &f_log_text) const;

    void postJsonWebhook(const QJsonDocument &f_json);
    void postMultipartWebhook(QHttpMultiPart &f_multipart);

    void onReplyFinished(QNetworkReply *f_reply);

    QNetworkAccessManager *m_nam;
    DiscordSettings *m_settings;
    QNetworkRequest m_request;
    AreaBufferFn m_area_buffer;
};

