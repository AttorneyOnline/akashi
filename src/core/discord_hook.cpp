#include "core/discord_hook.h"

#include "akashi/logging_categories.h"

#include <QHttpMultiPart>
#include <QHttpPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include <functional>

namespace akashi {

DiscordMessage &DiscordMessage::setRequestUrl(const QString &f_url)
{
    m_request_url = f_url;
    return *this;
}

DiscordMessage &DiscordMessage::setContent(const QString &f_content)
{
    m_fields[QStringLiteral("content")] = f_content;
    return *this;
}

DiscordMessage &DiscordMessage::setUsername(const QString &f_username)
{
    m_fields[QStringLiteral("username")] = f_username;
    return *this;
}

DiscordMessage &DiscordMessage::setAvatarUrl(const QString &f_avatar_url)
{
    m_fields[QStringLiteral("avatar_url")] = f_avatar_url;
    return *this;
}

DiscordMessage &DiscordMessage::setTts(bool f_tts)
{
    m_fields[QStringLiteral("tts")] = f_tts ? QStringLiteral("true") : QStringLiteral("false");
    return *this;
}

DiscordMessage &DiscordMessage::beginEmbed()
{
    if (m_building_embed) {
        endEmbed();
    }
    m_current_embed.clear();
    m_building_embed = true;
    return *this;
}

DiscordMessage &DiscordMessage::setEmbedTitle(const QString &f_title)
{
    if (m_building_embed) {
        m_current_embed[QStringLiteral("title")] = f_title;
    }
    return *this;
}

DiscordMessage &DiscordMessage::setEmbedDescription(const QString &f_description)
{
    if (m_building_embed) {
        m_current_embed[QStringLiteral("description")] = f_description;
    }
    return *this;
}

DiscordMessage &DiscordMessage::setEmbedUrl(const QString &f_url)
{
    if (m_building_embed) {
        m_current_embed[QStringLiteral("url")] = f_url;
    }
    return *this;
}

DiscordMessage &DiscordMessage::setEmbedColor(const QString &f_color)
{
    if (m_building_embed) {
        m_current_embed[QStringLiteral("color")] = f_color;
    }
    return *this;
}

DiscordMessage &DiscordMessage::setEmbedTimestamp(const QString &f_timestamp)
{
    if (m_building_embed) {
        m_current_embed[QStringLiteral("timestamp")] = f_timestamp;
    }
    return *this;
}

DiscordMessage &DiscordMessage::setEmbedFooter(const QString &f_text, const QString &f_icon_url)
{
    if (m_building_embed) {
        QVariantMap l_footer;
        l_footer[QStringLiteral("text")] = f_text;
        if (!f_icon_url.isEmpty()) {
            l_footer[QStringLiteral("icon_url")] = f_icon_url;
        }
        m_current_embed[QStringLiteral("footer")] = l_footer;
    }
    return *this;
}

DiscordMessage &DiscordMessage::setEmbedImage(const QString &f_url)
{
    if (m_building_embed) {
        QVariantMap l_image;
        l_image[QStringLiteral("url")] = f_url;
        m_current_embed[QStringLiteral("image")] = l_image;
    }
    return *this;
}

DiscordMessage &DiscordMessage::setEmbedThumbnail(const QString &f_url)
{
    if (m_building_embed) {
        QVariantMap l_thumbnail;
        l_thumbnail[QStringLiteral("url")] = f_url;
        m_current_embed[QStringLiteral("thumbnail")] = l_thumbnail;
    }
    return *this;
}

DiscordMessage &DiscordMessage::setEmbedAuthor(const QString &f_name, const QString &f_url, const QString &f_icon_url)
{
    if (m_building_embed) {
        QVariantMap l_author;
        l_author[QStringLiteral("name")] = f_name;
        if (!f_url.isEmpty()) {
            l_author[QStringLiteral("url")] = f_url;
        }
        if (!f_icon_url.isEmpty()) {
            l_author[QStringLiteral("icon_url")] = f_icon_url;
        }
        m_current_embed[QStringLiteral("author")] = l_author;
    }
    return *this;
}

DiscordMessage &DiscordMessage::addEmbedField(const QString &f_name, const QString &f_value, bool f_inline_field)
{
    if (m_building_embed) {
        QVariantMap l_field;
        l_field[QStringLiteral("name")] = f_name;
        l_field[QStringLiteral("value")] = f_value;
        l_field[QStringLiteral("inline")] = f_inline_field;

        QVariantList l_fields = m_current_embed[QStringLiteral("fields")].toList();
        l_fields.append(l_field);
        m_current_embed[QStringLiteral("fields")] = l_fields;
    }
    return *this;
}

DiscordMessage &DiscordMessage::endEmbed()
{
    if (m_building_embed) {
        m_embeds.append(m_current_embed);
        m_current_embed.clear();
        m_building_embed = false;
    }
    return *this;
}

QJsonObject DiscordMessage::toJson() const
{
    QJsonObject l_json;
    for (auto it = m_fields.constBegin(); it != m_fields.constEnd(); ++it) {
        if (!it.value().isNull()) {
            l_json[it.key()] = QJsonValue::fromVariant(it.value());
        }
    }

    if (!m_embeds.isEmpty()) {
        QJsonArray l_embeds;
        for (const QVariantMap &l_embed_map : m_embeds) {
            QJsonObject l_embed;
            for (auto it = l_embed_map.constBegin(); it != l_embed_map.constEnd(); ++it) {
                if (it.value().isNull()) {
                    continue;
                }
                // Nested maps carry the structured pieces: author, footer,
                // image, thumbnail.
                if (it.value().userType() == QMetaType::QVariantMap) {
                    QJsonObject l_nested;
                    const QVariantMap l_nested_map = it.value().toMap();
                    for (auto l_it = l_nested_map.constBegin(); l_it != l_nested_map.constEnd(); ++l_it) {
                        if (!l_it.value().isNull()) {
                            l_nested[l_it.key()] = QJsonValue::fromVariant(l_it.value());
                        }
                    }
                    l_embed[it.key()] = l_nested;
                }
                else {
                    l_embed[it.key()] = QJsonValue::fromVariant(it.value());
                }
            }
            if (!l_embed.isEmpty()) {
                l_embeds.append(l_embed);
            }
        }
        l_json[QStringLiteral("embeds")] = l_embeds;
    }

    return l_json;
}

DiscordMultipartMessage &DiscordMultipartMessage::setRequestUrl(const QString &f_url)
{
    m_request_url = f_url;
    return *this;
}

DiscordMultipartMessage &DiscordMultipartMessage::setPayloadJson(const QJsonObject &f_payload)
{
    m_payload_json = f_payload;
    return *this;
}

DiscordHook::DiscordHook(QNetworkAccessManager *f_network, QObject *parent) :
    QObject(parent),
    m_network(f_network)
{}

QString DiscordHook::serviceId() const
{
    return QStringLiteral("akashi.discordhook");
}

ServiceVersion DiscordHook::serviceVersion() const
{
    return {1, 0, 0};
}

void DiscordHook::post(const DiscordMessage &f_message)
{
    if (!m_network) {
        qCWarning(akashiDiscord) << "Cannot post: no network access manager installed";
        return;
    }
    const QUrl l_url(f_message.requestUrl());
    if (!l_url.isValid() || f_message.requestUrl().isEmpty()) {
        qCWarning(akashiDiscord) << "Cannot post: invalid webhook URL";
        return;
    }

    QNetworkRequest l_request(l_url);
    l_request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QNetworkReply *l_reply = m_network->post(l_request, QJsonDocument(f_message.toJson()).toJson());
    connect(l_reply, &QNetworkReply::finished, this, std::bind_front(&DiscordHook::onDiscordResponse, this, l_reply));
}

void DiscordHook::post(const DiscordMultipartMessage &f_message)
{
    if (!m_network) {
        qCWarning(akashiDiscord) << "Cannot post: no network access manager installed";
        return;
    }
    const QUrl l_url(f_message.requestUrl());
    if (!l_url.isValid() || f_message.requestUrl().isEmpty()) {
        qCWarning(akashiDiscord) << "Cannot post: invalid webhook URL";
        return;
    }

    auto *l_multipart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    for (const DiscordMultipart &l_part_data : f_message.parts()) {
        QHttpPart l_part;
        QString l_disposition = QStringLiteral("form-data; name=\"%1\"").arg(l_part_data.name);
        if (!l_part_data.filename.isEmpty()) {
            l_disposition += QStringLiteral("; filename=\"%1\"").arg(l_part_data.filename);
        }
        l_part.setHeader(QNetworkRequest::ContentDispositionHeader, l_disposition);

        if (!l_part_data.mime_type.isEmpty()) {
            QString l_content_type = l_part_data.mime_type;
            if (!l_part_data.charset.isEmpty()) {
                l_content_type += QStringLiteral("; charset=") + l_part_data.charset;
            }
            l_part.setHeader(QNetworkRequest::ContentTypeHeader, l_content_type);
        }

        l_part.setBody(l_part_data.data);
        l_multipart->append(l_part);
    }

    if (!f_message.payloadJson().isEmpty()) {
        QHttpPart l_json_part;
        l_json_part.setHeader(QNetworkRequest::ContentDispositionHeader, QStringLiteral("form-data; name=\"payload_json\""));
        l_json_part.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        l_json_part.setBody(QJsonDocument(f_message.payloadJson()).toJson(QJsonDocument::Compact));
        l_multipart->append(l_json_part);
    }

    QNetworkRequest l_request(l_url);
    QNetworkReply *l_reply = m_network->post(l_request, l_multipart);
    l_multipart->setParent(l_reply);
    connect(l_reply, &QNetworkReply::finished, this, std::bind_front(&DiscordHook::onDiscordResponse, this, l_reply));
}

void DiscordHook::onDiscordResponse(QNetworkReply *f_reply)
{
    f_reply->deleteLater();
    if (f_reply->error() != QNetworkReply::NoError) {
        qCWarning(akashiDiscord) << "Webhook failed:" << qUtf8Printable(f_reply->errorString());
        qCWarning(akashiDiscord) << "Response body:" << f_reply->readAll();
    }
}

} // namespace akashi
