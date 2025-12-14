#include "serverpublisher.h"
#include "servicewrapper.h"

#include <QDebug>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace {
constexpr int PUBLISHING_INTERVAL_MS = 1000 * 60 * 5;
}

AkashiPublisherError::AkashiPublisherError(const QString &f_data)
{
    QJsonDocument l_error_doc = QJsonDocument::fromJson(f_data.toUtf8());
}

ServerPublisher::ServerPublisher(ServiceRegistry *f_registry, QObject *parent)
    : Service{f_registry, parent}
    , m_publishing_interval{new QTimer(this)}
{
    ServiceWrapper<QNetworkAccessManager> *l_wrapper
        = m_registry->getService<ServiceWrapper<QNetworkAccessManager>>("qt.network.manager");
    m_network_manager = l_wrapper->instance();

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
    QNetworkReply *l_reply = m_network_manager->post(l_request,
                                                     QJsonDocument::fromVariant(m_properties)
                                                         .toJson(QJsonDocument::Compact));
    connect(l_reply, &QNetworkReply::finished, this, [this, l_reply]() { onServerListReply(l_reply); });
}

void ServerPublisher::onServerListReply(QNetworkReply *f_reply)
{
    QScopedPointer<QNetworkReply> l_reply(f_reply);
    if (l_reply->error() != QNetworkReply::NoError) {
        const QByteArray l_data = l_reply->readAll();
    }
}
