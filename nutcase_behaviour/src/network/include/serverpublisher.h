
#pragma once

#include "akashi_network_global.h"
#include "service.h"

#include <QVariantMap>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;
class ServiceRegistry;

struct AKASHI_NETWORK_EXPORT AkashiPublisherError
{
  public:
    AkashiPublisherError(const QString &f_data);

    QString title;
    QString type;
    QString keyword;
    QString errorString;
};

class AKASHI_NETWORK_EXPORT ServerPublisher : public Service
{
    Q_OBJECT

  public:
      ServerPublisher(ServiceRegistry *f_registry = nullptr, QObject *parent = nullptr);

      ServerPublisher &setProperty(const QString &f_key, const QVariant &f_value);
      void removeProperty(const QString &f_key);
      void setPublisherState(bool f_enabled);
      void setServerListUrl(const QString &f_url);

  Q_SIGNALS:
    void errorOccured(AkashiPublisherError f_error);

  public Q_SLOTS:
    void publishServer();

  private:
    void onServerListReply(QNetworkReply *f_reply);

    bool m_publishing_enabled = false;
    QString m_serverlist_url;
    QVariantMap m_properties;
    QTimer *m_publishing_interval = nullptr;
    QNetworkAccessManager *m_network_manager = nullptr;
    std::optional<AkashiPublisherError> m_last_error;
};
