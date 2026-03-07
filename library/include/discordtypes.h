#pragma once

#include "akashi_addon_global.h"

#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QVector>
#include <concepts>

class DiscordMessageCommon
{
  public:
    const QString &requestUrl() const { return m_request_url; }

  protected:
    QString m_request_url;
};

class AKASHI_ADDON_EXPORT DiscordMessage : public DiscordMessageCommon
{
  public:
    DiscordMessage &setRequestUrl(const QString &url);
    DiscordMessage &setContent(const QString &content);
    DiscordMessage &setUsername(const QString &username);
    DiscordMessage &setAvatarUrl(const QString &avatar_url);
    DiscordMessage &setTts(bool tts);

    DiscordMessage &beginEmbed();
    DiscordMessage &setEmbedTitle(const QString &title);
    DiscordMessage &setEmbedDescription(const QString &description);
    DiscordMessage &setEmbedUrl(const QString &url);
    DiscordMessage &setEmbedColor(QString color);
    DiscordMessage &setEmbedTimestamp(const QString &timestamp);
    DiscordMessage &setEmbedFooter(const QString &text, const QString &icon_url = "");
    DiscordMessage &setEmbedImage(const QString &url);
    DiscordMessage &setEmbedThumbnail(const QString &url);
    DiscordMessage &setEmbedAuthor(const QString &name, const QString &url = "", const QString &icon_url = "");
    DiscordMessage &addEmbedField(const QString &name, const QString &value, bool inline_field = false);
    DiscordMessage &endEmbed();

    QJsonObject toJson() const;

  private:
    QMap<QString, QString> m_fields;
    QVector<QVariantMap> m_embeds;
    QVariantMap m_current_embed;
    bool m_building_embed = false;
};

struct DiscordMultipart
{
    QByteArray data;
    QString name;
    QString filename;
    QString mime_type;
    QString charset;

    template <typename T>
        requires std::convertible_to<T, QByteArray>
    DiscordMultipart(T data, QString name, QString filename = "",
                     QString mime_type = "", QString charset = "") : data(std::move(data)),
                                                                     name(std::move(name)),
                                                                     filename(std::move(filename)),
                                                                     mime_type(std::move(mime_type)),
                                                                     charset(std::move(charset))
    {}
};

class AKASHI_ADDON_EXPORT DiscordMultipartMessage : public DiscordMessageCommon
{
  public:
    template <typename T>
    DiscordMultipartMessage &addPart(T data, QString name, QString filename = "", QString mime_type = "", QString charset = "")
    {
        m_parts.append(DiscordMultipart(std::move(data), std::move(name), std::move(filename), std::move(mime_type), std::move(charset)));
        return *this;
    }

    DiscordMultipartMessage &setRequestUrl(const QString &url);
    DiscordMultipartMessage &setPayloadJson(const QJsonObject &payload);

    int size() const { return m_parts.size(); }
    const DiscordMultipart &partAt(int index) const { return m_parts.at(index); }
    const QVector<DiscordMultipart> &parts() const { return m_parts; }
    const QJsonObject &payloadJson() const { return m_payload_json; }

  private:
    QVector<DiscordMultipart> m_parts;
    QJsonObject m_payload_json;
};
