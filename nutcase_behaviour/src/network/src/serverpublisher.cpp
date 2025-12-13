#include "serverpublisher.h"

#include <QDebug>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace {
constexpr int PUBLISHING_INTERVAL_MS = 1000 * 60 * 5;
}

ServerPublisher::ServerPublisher(QNetworkAccessManager *f_net_man,
                                 ServiceRegistry *f_registry,
                                 QObject *parent) : Service{f_registry, parent},
                                                    m_net_man{f_net_man},
                                                    m_publishing_interval{new QTimer(this)}
{
    m_publishing_interval->setInterval(PUBLISHING_INTERVAL_MS);
    m_publishing_interval->callOnTimeout(this, &ServerPublisher::publishServer);
}

ServerPublisher &ServerPublisher::setProperty(const QString &f_key, const QVariant &f_value)
{
    m_properties.insert(f_key, f_value);
    return *this;
}

void ServerPublisher::removeProperty(const QString &f_key)
{
    m_properties.remove(f_key);
}

void ServerPublisher::setPublisherState(bool f_enabled)
{
    m_publishing_enabled = f_enabled;
    if (m_publishing_enabled) {
        publishServer();
    }
}

void ServerPublisher::setServerListUrl(const QString &f_url)
{
    m_serverlist_url = f_url;
}

void ServerPublisher::publishServer()
{
    if (!m_publishing_enabled) {
        return;
    }

    QNetworkRequest l_request(m_serverlist_url);
    if (!l_request.url().isValid()) {
        qWarning() << "Failed to publish server: invalid URL";
        return;
    }
    m_publishing_interval->start();

    l_request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *l_reply = m_net_man->post(l_request, QJsonDocument::fromVariant(m_properties).toJson(QJsonDocument::Compact));
    connect(l_reply, &QNetworkReply::finished, this, [this, l_reply]() { onServerListReply(l_reply); });
}

void ServerPublisher::onServerListReply(QNetworkReply *f_reply)
{
    QScopedPointer<QNetworkReply> l_reply(f_reply);
    if (l_reply->error() != QNetworkReply::NoError) {
        qWarning() << "Publish failed:" << l_reply->errorString();
    }
    else {
        qInfo() << "Published server to serverlist.";
    }
}
