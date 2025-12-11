#pragma once

#include "akashi_core_global.h"
#include "service.h"

#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QVector>
#include <concepts>

class QNetworkAccessManager;
class QNetworkReply;

class DiscordMessageCommon
{
  public:
    const QString &requestUrl() const { return m_request_url; }

  protected:
    QString m_request_url;
};

class DiscordMessage : public DiscordMessageCommon
{
  public:
    DiscordMessage() = default;
    ~DiscordMessage() = default;

    DiscordMessage &setRequestUrl(const QString &url);
    DiscordMessage &setContent(const QString &content);
    DiscordMessage &setUsername(const QString &username);
    DiscordMessage &setAvatarUrl(const QString &avatar_url);
    DiscordMessage &setTts(bool tts);

    DiscordMessage &beginEmbed();
    DiscordMessage &setEmbedTitle(const QString &title);
    DiscordMessage &setEmbedDescription(const QString &description);
    DiscordMessage &setEmbedUrl(const QString &url);
    DiscordMessage &setEmbedColor(int color);
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
    QVector<QMap<QString, QVariant>> m_embeds;
    QMap<QString, QVariant> m_current_embed;
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

class DiscordMultipartMessage : public DiscordMessageCommon
{
  public:
    DiscordMultipartMessage() = default;
    ~DiscordMultipartMessage() = default;

    template <typename T>
    DiscordMultipartMessage &addPart(T data, QString name, QString filename = "")
    {
        m_parts.append(DiscordMultipart(std::move(data), std::move(name), std::move(filename)));
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

class DiscordHook : public Service
{
    Q_OBJECT
    Q_CLASSINFO("Author", "Salanto")
    Q_CLASSINFO("Version", "1.0.0")
    Q_CLASSINFO("Identifier", "akashi.discordhook")

  public:
    DiscordHook(ServiceRegistry *registry, QObject *parent = nullptr);
    ~DiscordHook() = default;

    void installNetworkManager(QNetworkAccessManager *network_manager);

    void post(const DiscordMessage &message);
    void post(const DiscordMultipartMessage &message);

  private slots:
    void onDiscordResponse(QNetworkReply *reply);

  private:
    QNetworkAccessManager *m_network_manager = nullptr;
};
