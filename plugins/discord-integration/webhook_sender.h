#pragma once

#include "webhook_payloads.h"

#include <QJsonDocument>
#include <QNetworkRequest>
#include <QString>

#include <functional>

class QNetworkAccessManager;

class WebhookSender : public QObject
{
    Q_OBJECT

  public:
    using AreaLogFn = std::function<QString(const QString &)>;

    WebhookSender(QNetworkAccessManager *f_nam, QObject *parent = nullptr);

    void setModcallUrl(const QString &f_url);
    void setModcallContent(const QString &f_content);
    void setModcallSendfile(bool f_send);
    void setBanUrl(const QString &f_url);
    void setColor(const QString &f_color);
    void setAreaLogFn(AreaLogFn f_fn);

    void sendModcall(const ModcallPayload &f_payload);
    void sendBan(const BanPayload &f_payload);

  private:
    QJsonDocument buildModcallJson(const ModcallPayload &f_payload) const;
    QJsonDocument buildBanJson(const BanPayload &f_payload) const;

    void postJson(const QUrl &f_url, const QJsonDocument &f_json);
    void postLogFile(const QUrl &f_url, const QString &f_log_text);

    QNetworkAccessManager *m_nam = nullptr;
    QString m_modcall_url;
    QString m_modcall_content;
    bool m_modcall_sendfile = false;
    QString m_ban_url;
    QString m_color;
    AreaLogFn m_area_log_fn;
};
