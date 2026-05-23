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
#include "discord.h"

#include "akashi/event.h"
#include "core/event_bus.h"
#include "core/server_settings.h"

Discord::Discord(DiscordSettings *f_settings, akashi::EventBus *f_event_bus,
                 AreaBufferFn f_area_buffer, QObject *parent) :
    QObject(parent),
    m_settings(f_settings),
    m_area_buffer(std::move(f_area_buffer))
{
    m_nam = new QNetworkAccessManager();
    connect(m_nam, &QNetworkAccessManager::finished,
            this, &Discord::onReplyFinished);

    f_event_bus->subscribe<akashi::ModcallEvent>(
        akashi::EventPhase::After, 0,
        [this](akashi::ModcallEvent &e) { onModcall(e); },
        QStringLiteral("discord"));

    f_event_bus->subscribe<akashi::BanIssuedEvent>(
        akashi::EventPhase::After, 0,
        [this](akashi::BanIssuedEvent &e) { onBan(e); },
        QStringLiteral("discord"));
}

void Discord::onModcall(akashi::ModcallEvent &f_event)
{
    if (!m_settings->webhook_enabled() || !m_settings->webhook_modcall_enabled()) {
        return;
    }

    QString l_name = f_event.ooc_name;
    if (l_name.isEmpty()) {
        l_name = f_event.char_name;
    }

    m_request.setUrl(QUrl(m_settings->webhook_modcall_url()));
    QJsonDocument l_json = constructModcallJson(l_name, f_event.area_name,
                                                 QString::number(f_event.client_id), f_event.reason);
    postJsonWebhook(l_json);

    if (m_settings->webhook_modcall_sendfile() && m_area_buffer) {
        const QString l_log_text = m_area_buffer(f_event.area_name);
        if (!l_log_text.isEmpty()) {
            QHttpMultiPart *l_multipart = constructLogMultipart(l_log_text);
            postMultipartWebhook(*l_multipart);
        }
    }
}

void Discord::onBan(akashi::BanIssuedEvent &f_event)
{
    if (!m_settings->webhook_enabled() || !m_settings->webhook_ban_enabled()) {
        return;
    }

    m_request.setUrl(QUrl(m_settings->webhook_ban_url()));
    QJsonDocument l_json = constructBanJson(f_event.target_ipid, f_event.moderator,
                                             f_event.duration, f_event.reason, f_event.ban_id);
    postJsonWebhook(l_json);
}

QJsonDocument Discord::constructModcallJson(const QString &f_name, const QString &f_area, const QString &f_id, const QString &f_reason) const
{
    QJsonObject l_json;
    QJsonArray l_array;
    QJsonObject l_object{
        {"color", m_settings->webhook_color()},
        {"title", "[" + f_id + "]" + f_name + " filed a modcall in " + f_area},
        {"description", f_reason}};
    l_array.append(l_object);

    if (!m_settings->webhook_modcall_content().isEmpty())
        l_json["content"] = m_settings->webhook_modcall_content();
    l_json["embeds"] = l_array;

    return QJsonDocument(l_json);
}

QJsonDocument Discord::constructBanJson(const QString &f_ipid, const QString &f_moderator, const QString &f_duration, const QString &f_reason, int f_ban_id)
{
    QJsonObject l_json;
    QJsonArray l_array;
    QJsonObject l_object{
        {"color", m_settings->webhook_color()},
        {"title", "Ban issued by " + f_moderator},
        {"description", "Client IPID : " + f_ipid + "\nBan ID: " + QString::number(f_ban_id) + "\nBan reason : " + f_reason + "\nBanned until : " + f_duration}};
    l_array.append(l_object);
    l_json["embeds"] = l_array;

    return QJsonDocument(l_json);
}

QHttpMultiPart *Discord::constructLogMultipart(const QString &f_log_text) const
{
    QHttpMultiPart *l_multipart = new QHttpMultiPart();
    QHttpPart l_logdata;
    l_logdata.setHeader(QNetworkRequest::ContentDispositionHeader, "form-data; name=\"file\"; filename=\"log.txt\"");
    l_logdata.setHeader(QNetworkRequest::ContentTypeHeader, "text/plain; charset=utf-8");
    l_logdata.setBody(f_log_text.toUtf8());
    l_multipart->append(l_logdata);
    return l_multipart;
}

void Discord::postJsonWebhook(const QJsonDocument &f_json)
{
    if (!QUrl(m_request.url()).isValid()) {
        qWarning("Invalid webhook URL!");
        return;
    }
    m_request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    m_nam->post(m_request, f_json.toJson());
}

void Discord::postMultipartWebhook(QHttpMultiPart &f_multipart)
{
    if (!QUrl(m_request.url()).isValid()) {
        qWarning("Invalid webhook URL!");
        f_multipart.deleteLater();
        return;
    }
    m_request.setHeader(QNetworkRequest::ContentTypeHeader, "multipart/form-data; boundary=" + f_multipart.boundary());
    QNetworkReply *l_reply = m_nam->post(m_request, &f_multipart);
    f_multipart.setParent(l_reply);
}

void Discord::onReplyFinished(QNetworkReply *f_reply)
{
    auto l_data = f_reply->readAll();
    f_reply->deleteLater();
#ifdef DISCORD_DEBUG
    QDebug() << l_data;
#else
    Q_UNUSED(l_data);
#endif
}

Discord::~Discord()
{
    m_nam->deleteLater();
}
