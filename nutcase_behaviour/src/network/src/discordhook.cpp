#include "discordhook.h"
#include "servicewrapper.h"

#include <QHttpMultiPart>
#include <QHttpPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

DiscordMessage &DiscordMessage::setRequestUrl(const QString &url)
{
    m_request_url = url;
    return *this;
}

DiscordMessage &DiscordMessage::setContent(const QString &content)
{
    m_fields["content"] = content;
    return *this;
}

DiscordMessage &DiscordMessage::setUsername(const QString &username)
{
    m_fields["username"] = username;
    return *this;
}

DiscordMessage &DiscordMessage::setAvatarUrl(const QString &avatar_url)
{
    m_fields["avatar_url"] = avatar_url;
    return *this;
}

DiscordMessage &DiscordMessage::setTts(bool tts)
{
    m_fields["tts"] = tts ? "true" : "false";
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

DiscordMessage &DiscordMessage::setEmbedTitle(const QString &title)
{
    if (m_building_embed) {
        m_current_embed["title"] = title;
    }
    return *this;
}

DiscordMessage &DiscordMessage::setEmbedDescription(const QString &description)
{
    if (m_building_embed) {
        m_current_embed["description"] = description;
    }
    return *this;
}

DiscordMessage &DiscordMessage::setEmbedUrl(const QString &url)
{
    if (m_building_embed) {
        m_current_embed["url"] = url;
    }
    return *this;
}

DiscordMessage &DiscordMessage::setEmbedColor(int color)
{
    if (m_building_embed) {
        m_current_embed["color"] = color;
    }
    return *this;
}

DiscordMessage &DiscordMessage::setEmbedTimestamp(const QString &timestamp)
{
    if (m_building_embed) {
        m_current_embed["timestamp"] = timestamp;
    }
    return *this;
}

DiscordMessage &DiscordMessage::setEmbedFooter(const QString &text, const QString &icon_url)
{
    if (m_building_embed) {
        QMap<QString, QVariant> l_footer;
        l_footer["text"] = text;
        if (!icon_url.isEmpty()) {
            l_footer["icon_url"] = icon_url;
        }
        m_current_embed["footer"] = QVariant::fromValue(l_footer);
    }
    return *this;
}

DiscordMessage &DiscordMessage::setEmbedImage(const QString &url)
{
    if (m_building_embed) {
        QMap<QString, QVariant> l_image;
        l_image["url"] = url;
        m_current_embed["image"] = QVariant::fromValue(l_image);
    }
    return *this;
}

DiscordMessage &DiscordMessage::setEmbedThumbnail(const QString &url)
{
    if (m_building_embed) {
        QMap<QString, QVariant> l_thumbnail;
        l_thumbnail["url"] = url;
        m_current_embed["thumbnail"] = QVariant::fromValue(l_thumbnail);
    }
    return *this;
}

DiscordMessage &DiscordMessage::setEmbedAuthor(const QString &name, const QString &url, const QString &icon_url)
{
    if (m_building_embed) {
        QMap<QString, QVariant> l_author;
        l_author["name"] = name;
        if (!url.isEmpty()) {
            l_author["url"] = url;
        }
        if (!icon_url.isEmpty()) {
            l_author["icon_url"] = icon_url;
        }
        m_current_embed["author"] = QVariant::fromValue(l_author);
    }
    return *this;
}

DiscordMessage &DiscordMessage::addEmbedField(const QString &name, const QString &value, bool inline_field)
{
    if (m_building_embed) {
        QMap<QString, QVariant> l_field;
        l_field["name"] = name;
        l_field["value"] = value;
        l_field["inline"] = inline_field;

        QVariantList l_fields;
        if (m_current_embed.contains("fields")) {
            l_fields = m_current_embed["fields"].toList();
        }
        l_fields.append(QVariant::fromValue(l_field));
        m_current_embed["fields"] = l_fields;
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

    for (auto it = m_fields.begin(); it != m_fields.end(); ++it) {
        const QString &key = it.key();
        const QString &value = it.value();

        if (key == "tts") {
            l_json[key] = (value == "true");
        }
        else {
            l_json[key] = value;
        }
    }

    if (!m_embeds.isEmpty()) {
        QJsonArray l_embeds_array;

        for (const auto &l_embed_map : m_embeds) {
            QJsonObject l_embed_obj;

            for (auto it = l_embed_map.begin(); it != l_embed_map.end(); ++it) {
                const QString &l_key = it.key();
                const QVariant &l_value = it.value();

                if (l_value.canConvert<QVariantMap>()) {
                    QJsonObject l_nested_obj;
                    QVariantMap l_nested_map = l_value.toMap();
                    for (auto nested_it = l_nested_map.begin(); nested_it != l_nested_map.end();
                         ++nested_it) {
                        l_nested_obj[nested_it.key()] = QJsonValue::fromVariant(nested_it.value());
                    }
                    l_embed_obj[l_key] = l_nested_obj;
                } else if (l_value.canConvert<QVariantList>()) {
                    QJsonArray fields_array;
                    const QVariantList l_fields_list = l_value.toList();
                    for (const auto &l_field_variant : l_fields_list) {
                        QJsonObject l_field_obj;
                        QVariantMap l_field_map = l_field_variant.toMap();
                        for (auto field_it = l_field_map.begin(); field_it != l_field_map.end();
                             ++field_it) {
                            l_field_obj[field_it.key()] = QJsonValue::fromVariant(field_it.value());
                        }
                        fields_array.append(l_field_obj);
                    }
                    l_embed_obj[l_key] = fields_array;
                } else {
                    l_embed_obj[l_key] = QJsonValue::fromVariant(l_value);
                }
            }

            l_embeds_array.append(l_embed_obj);
        }

        l_json["embeds"] = l_embeds_array;
    }

    return l_json;
}

DiscordMultipartMessage &DiscordMultipartMessage::setRequestUrl(const QString &url)
{
    m_request_url = url;
    return *this;
}

DiscordMultipartMessage &DiscordMultipartMessage::setPayloadJson(const QJsonObject &payload)
{
    m_payload_json = payload;
    return *this;
}

DiscordHook::DiscordHook(ServiceRegistry *registry, QObject *parent)
    : Service{registry, parent}
{
    m_service_properties = {{"author", "Salanto"},
                            {"version", "1.0.0"},
                            {"identifier", "akashi.network.discordhook"}};

    ServiceWrapper<QNetworkAccessManager> *l_wrapper
        = m_registry->getService<ServiceWrapper<QNetworkAccessManager>>("qt.network.manager");
    m_network_manager = l_wrapper->instance();
    m_registry->registerService(this);
}

void DiscordHook::post(const DiscordMessage &message)
{
    if (!m_network_manager) {
        qWarning() << "Cannot post DiscordMessage: QNetworkAccessManager not installed";
        return;
    }

    QUrl url(message.requestUrl());
    if (!url.isValid()) {
        qWarning() << "Failed to post DiscordMessage: Invalid URL" << message.requestUrl();
        return;
    }

    QJsonObject json = message.toJson();

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = m_network_manager->post(request, QJsonDocument(json).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onDiscordResponse(reply);
    });
}

void DiscordHook::post(const DiscordMultipartMessage &message)
{
    if (!m_network_manager) {
        qWarning() << "Cannot post DiscordMultipartMessage: QNetworkAccessManager not installed";
        return;
    }

    QUrl url(message.requestUrl());
    if (!url.isValid()) {
        qWarning() << "Failed to post DiscordMultipartMessage: Invalid URL" << message.requestUrl();
        return;
    }

    auto *multipart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    if (!message.payloadJson().isEmpty()) {
        QHttpPart json_part;
        json_part.setHeader(QNetworkRequest::ContentDispositionHeader, "form-data; name=\"payload_json\"");
        json_part.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        json_part.setBody(QJsonDocument(message.payloadJson()).toJson(QJsonDocument::Compact));
        multipart->append(json_part);
    }

    for (int i = 0; i < message.size(); ++i) {
        const DiscordMultipart &part_data = message.partAt(i);

        QHttpPart http_part;

        QString disposition = QString("form-data; name=\"%1\"").arg(part_data.name);
        if (!part_data.filename.isEmpty()) {
            disposition += QString("; filename=\"%1\"").arg(part_data.filename);
        }
        http_part.setHeader(QNetworkRequest::ContentDispositionHeader, disposition);

        if (!part_data.mime_type.isEmpty()) {
            QString content_type = part_data.mime_type;
            if (!part_data.charset.isEmpty()) {
                content_type += "; charset=" + part_data.charset;
            }
            http_part.setHeader(QNetworkRequest::ContentTypeHeader, content_type);
        }

        http_part.setBody(part_data.data);
        multipart->append(http_part);
    }

    QNetworkRequest request(url);

    QNetworkReply *reply = m_network_manager->post(request, multipart);
    multipart->setParent(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onDiscordResponse(reply);
    });
}

void DiscordHook::onDiscordResponse(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Discord webhook failed:" << reply->errorString();
        qWarning() << "Response body:" << reply->readAll();
        return;
    }
}
