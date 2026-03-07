#include "discordhook.h"
#include "serviceregistry.h"
#include "servicewrapper.h"

#include <QHttpMultiPart>
#include <QHttpPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

Q_LOGGING_CATEGORY(akashiDiscordHook, "akashi.addon.discordhook")

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

DiscordMessage &DiscordMessage::setEmbedColor(QString color)
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
    QJsonObject json;

    // Add all fields (including content)
    for (auto it = m_fields.constBegin(); it != m_fields.constEnd(); ++it) {
        if (!it.value().isNull()) {
            json[it.key()] = QJsonValue::fromVariant(it.value());
        }
    }

    // Handle embeds
    if (!m_embeds.isEmpty()) {
        QJsonArray embedsArray;

        for (const QVariantMap &embedMap : m_embeds) {
            QJsonObject embedObj;

            for (auto it = embedMap.constBegin(); it != embedMap.constEnd(); ++it) {
                if (it.value().isNull()) {
                    continue;
                }

                // Handle nested maps (e.g., author, footer, etc.)
                if (it.value().canConvert<QVariantMap>()) {
                    QJsonObject nestedObj;
                    QVariantMap nestedMap = it.value().toMap();

                    for (auto nestedIt = nestedMap.constBegin();
                         nestedIt != nestedMap.constEnd();
                         ++nestedIt) {
                        if (!nestedIt.value().isNull()) {
                            nestedObj[nestedIt.key()] = QJsonValue::fromVariant(nestedIt.value());
                        }
                    }

                    embedObj[it.key()] = nestedObj;
                }
                else {
                    embedObj[it.key()] = QJsonValue::fromVariant(it.value());
                }
            }

            if (!embedObj.isEmpty()) {
                embedsArray.append(embedObj);
            }
        }

        json["embeds"] = embedsArray;
    }

    return json;
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

DiscordHook::DiscordHook(QObject *parent) : Service{parent}
{
    m_service_properties = {{"author", "Salanto"},
                            {"version", "1.0.0"},
                            {"identifier", SERVICE_ID}};
}

void DiscordHook::setServiceRegistry(ServiceRegistry *f_registry)
{
    m_registry = f_registry;

    auto l_service = m_registry->get<ServiceWrapper<QNetworkAccessManager>>("qt.network.manager");
    if (!l_service.has_value()) {
        setState(State::FAILED);
    }

    setState(State::OK);
    m_network_manager = l_service.value()->get();
}

void DiscordHook::post(const DiscordMessage &message)
{
    if (!m_network_manager) {
        qCWarning(akashiDiscordHook) << "Cannot post DiscordMessage: QNetworkAccessManager not installed";
        return;
    }

    QUrl url(message.requestUrl());
    if (!url.isValid()) {
        qCWarning(akashiDiscordHook) << "Failed to post DiscordMessage: Invalid URL" << qUtf8Printable(message.requestUrl());
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
    QUrl url(message.requestUrl());
    if (!url.isValid()) {
        qCWarning(akashiDiscordHook) << "Failed to post DiscordMultipartMessage: Invalid URL" << qUtf8Printable(message.requestUrl());
        return;
    }

    auto *multipart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

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

    if (!message.payloadJson().isEmpty()) {
        QHttpPart json_part;
        json_part.setHeader(QNetworkRequest::ContentDispositionHeader, "form-data; name=\"payload_json\"");
        json_part.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        json_part.setBody(QJsonDocument(message.payloadJson()).toJson(QJsonDocument::Compact));
        multipart->append(json_part);
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
        qCWarning(akashiDiscordHook) << "Discord webhook failed:" << qUtf8Printable(reply->errorString());
        qCWarning(akashiDiscordHook) << "Response body:" << reply->readAll();
        return;
    }
}
