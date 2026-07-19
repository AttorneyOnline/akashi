#pragma once

#include "akashi_core_export.h"

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QVariantMap>
#include <QVector>

#include <concepts>

namespace akashi {

// Exported alongside its derived classes so the shared base does not cross the
// DLL boundary as an unexported symbol (MSVC C4275).
class AKASHI_CORE_EXPORT DiscordMessageCommon
{
  public:
    const QString &requestUrl() const { return m_request_url; }

  protected:
    QString m_request_url;
};

// A Discord webhook message, built fluently: top-level fields first, then
// any number of beginEmbed()...endEmbed() blocks with the embed's pieces
// in between. toJson() produces the payload Discord expects.
class AKASHI_CORE_EXPORT DiscordMessage : public DiscordMessageCommon
{
  public:
    DiscordMessage &setRequestUrl(const QString &f_url);
    DiscordMessage &setContent(const QString &f_content);
    DiscordMessage &setUsername(const QString &f_username);
    DiscordMessage &setAvatarUrl(const QString &f_avatar_url);
    DiscordMessage &setTts(bool f_tts);

    DiscordMessage &beginEmbed();
    DiscordMessage &setEmbedTitle(const QString &f_title);
    DiscordMessage &setEmbedDescription(const QString &f_description);
    DiscordMessage &setEmbedUrl(const QString &f_url);
    DiscordMessage &setEmbedColor(const QString &f_color);
    DiscordMessage &setEmbedTimestamp(const QString &f_timestamp);
    DiscordMessage &setEmbedFooter(const QString &f_text, const QString &f_icon_url = {});
    DiscordMessage &setEmbedImage(const QString &f_url);
    DiscordMessage &setEmbedThumbnail(const QString &f_url);
    DiscordMessage &setEmbedAuthor(const QString &f_name, const QString &f_url = {}, const QString &f_icon_url = {});
    DiscordMessage &addEmbedField(const QString &f_name, const QString &f_value, bool f_inline_field = false);
    DiscordMessage &endEmbed();

    QJsonObject toJson() const;

  private:
    QHash<QString, QString> m_fields;
    QVector<QVariantMap> m_embeds;
    QVariantMap m_current_embed;
    bool m_building_embed = false;
};

// One part of a multipart message, usually a file attachment.
struct DiscordMultipart
{
    QByteArray data;
    QString name;
    QString filename;
    QString mime_type;
    QString charset;

    template <typename T>
        requires std::convertible_to<T, QByteArray>
    DiscordMultipart(T f_data, QString f_name, QString f_filename = {},
                     QString f_mime_type = {}, QString f_charset = {}) :
        data(std::move(f_data)),
        name(std::move(f_name)),
        filename(std::move(f_filename)),
        mime_type(std::move(f_mime_type)),
        charset(std::move(f_charset))
    {}
};

// A multipart webhook message: file parts plus an optional payload built
// from a DiscordMessage's toJson().
class AKASHI_CORE_EXPORT DiscordMultipartMessage : public DiscordMessageCommon
{
  public:
    template <typename T>
    DiscordMultipartMessage &addPart(T f_data, QString f_name, QString f_filename = {},
                                     QString f_mime_type = {}, QString f_charset = {})
    {
        m_parts.append(DiscordMultipart(std::move(f_data), std::move(f_name), std::move(f_filename),
                                        std::move(f_mime_type), std::move(f_charset)));
        return *this;
    }

    DiscordMultipartMessage &setRequestUrl(const QString &f_url);
    DiscordMultipartMessage &setPayloadJson(const QJsonObject &f_payload);

    int size() const { return m_parts.size(); }
    const DiscordMultipart &partAt(int f_index) const { return m_parts.at(f_index); }
    const QVector<DiscordMultipart> &parts() const { return m_parts; }
    const QJsonObject &payloadJson() const { return m_payload_json; }

  private:
    QVector<DiscordMultipart> m_parts;
    QJsonObject m_payload_json;
};

} // namespace akashi
