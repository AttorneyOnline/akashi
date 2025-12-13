
#pragma once

#include "akashi_network_global.h"
#include "service.h"

#include <QVariantMap>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;
class ServiceRegistry;

class AKASHI_NETWORK_EXPORT ServerPublisher : public Service
{
    Q_OBJECT

  public:
    ServerPublisher(QNetworkAccessManager *f_net_man = nullptr,
                    ServiceRegistry *f_registry = nullptr,
                    QObject *parent = nullptr);

    ServerPublisher &setProperty(const QString &f_key, const QVariant &f_value);
    void removeProperty(const QString &f_key);
    void setPublisherState(bool f_enabled);
    void setServerListUrl(const QString &f_url);

  public slots:
    void publishServer();

  private:
    void onServerListReply(QNetworkReply *f_reply);

    bool m_publishing_enabled = false;
    QString m_serverlist_url;
    QVariantMap m_properties;
    QTimer *m_publishing_interval = nullptr;
    QNetworkAccessManager *m_net_man = nullptr;
};
