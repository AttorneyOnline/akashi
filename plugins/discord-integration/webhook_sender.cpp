#include "webhook_sender.h"

#include <QHttpMultiPart>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>

WebhookSender::WebhookSender(QNetworkAccessManager *f_nam, QObject *parent) :
    QObject(parent),
    m_nam(f_nam)
{
}

void WebhookSender::setModcallUrl(const QString &f_url) { m_modcall_url = f_url; }
void WebhookSender::setModcallContent(const QString &f_content) { m_modcall_content = f_content; }
void WebhookSender::setModcallSendfile(bool f_send) { m_modcall_sendfile = f_send; }
void WebhookSender::setBanUrl(const QString &f_url) { m_ban_url = f_url; }
void WebhookSender::setColor(const QString &f_color) { m_color = f_color; }
void WebhookSender::setAreaLogFn(AreaLogFn f_fn) { m_area_log_fn = std::move(f_fn); }

void WebhookSender::sendModcall(const ModcallPayload &f_payload)
{
    QUrl l_url(m_modcall_url);
    if (!l_url.isValid() || m_modcall_url.isEmpty())
        return;

    postJson(l_url, buildModcallJson(f_payload));

    if (m_modcall_sendfile && m_area_log_fn) {
        const QString l_log = m_area_log_fn(f_payload.area_name);
        if (!l_log.isEmpty())
            postLogFile(l_url, l_log);
    }
}

void WebhookSender::sendBan(const BanPayload &f_payload)
{
    QUrl l_url(m_ban_url);
    if (!l_url.isValid() || m_ban_url.isEmpty())
        return;

    postJson(l_url, buildBanJson(f_payload));
}

QJsonDocument WebhookSender::buildModcallJson(const ModcallPayload &f_payload) const
{
    QJsonObject l_embed{
        {QStringLiteral("color"), m_color},
        {QStringLiteral("title"),
         QStringLiteral("[%1]%2 filed a modcall in %3")
             .arg(QString::number(f_payload.client_id), f_payload.name, f_payload.area_name)},
        {QStringLiteral("description"), f_payload.reason}};

    QJsonObject l_root;
    if (!m_modcall_content.isEmpty())
        l_root[QStringLiteral("content")] = m_modcall_content;
    l_root[QStringLiteral("embeds")] = QJsonArray{l_embed};
    return QJsonDocument(l_root);
}

QJsonDocument WebhookSender::buildBanJson(const BanPayload &f_payload) const
{
    QJsonObject l_embed{
        {QStringLiteral("color"), m_color},
        {QStringLiteral("title"), QStringLiteral("Ban issued by ") + f_payload.moderator},
        {QStringLiteral("description"),
         QStringLiteral("Client IPID : %1\nBan ID: %2\nBan reason : %3\nBanned until : %4")
             .arg(f_payload.target_ipid, QString::number(f_payload.ban_id),
                  f_payload.reason, f_payload.duration)}};

    QJsonObject l_root;
    l_root[QStringLiteral("embeds")] = QJsonArray{l_embed};
    return QJsonDocument(l_root);
}

void WebhookSender::postJson(const QUrl &f_url, const QJsonDocument &f_json)
{
    QNetworkRequest l_request(f_url);
    l_request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QNetworkReply *l_reply = m_nam->post(l_request, f_json.toJson());
    connect(l_reply, &QNetworkReply::finished, l_reply, &QNetworkReply::deleteLater);
}

void WebhookSender::postLogFile(const QUrl &f_url, const QString &f_log_text)
{
    auto *l_multipart = new QHttpMultiPart();
    QHttpPart l_part;
    l_part.setHeader(QNetworkRequest::ContentDispositionHeader,
                     QStringLiteral("form-data; name=\"file\"; filename=\"log.txt\""));
    l_part.setHeader(QNetworkRequest::ContentTypeHeader,
                     QStringLiteral("text/plain; charset=utf-8"));
    l_part.setBody(f_log_text.toUtf8());
    l_multipart->append(l_part);

    QNetworkRequest l_request(f_url);
    l_request.setHeader(QNetworkRequest::ContentTypeHeader,
                        QStringLiteral("multipart/form-data; boundary=") + l_multipart->boundary());
    QNetworkReply *l_reply = m_nam->post(l_request, l_multipart);
    l_multipart->setParent(l_reply);
    connect(l_reply, &QNetworkReply::finished, l_reply, &QNetworkReply::deleteLater);
}
