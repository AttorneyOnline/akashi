#pragma once

#include "akashi_addon_global.h"
#include "discordtypes.h"
#include "service.h"

class QNetworkAccessManager;
class QNetworkReply;

Q_DECLARE_EXPORTED_LOGGING_CATEGORY(akashiDiscordHook, AKASHI_ADDON_EXPORT)
class AKASHI_ADDON_EXPORT DiscordHook : public Service
{
    Q_OBJECT

  public:
    DiscordHook(QObject *parent = nullptr);
    ~DiscordHook() = default;

    inline const static QString SERVICE_ID = "akashi.addon.discordook";

    void
    setServiceRegistry(ServiceRegistry *f_registry = nullptr) override;

    void
    post(const DiscordMessage &message);
    void post(const DiscordMultipartMessage &message);

  private slots:
    void onDiscordResponse(QNetworkReply *reply);

  private:
    QNetworkAccessManager *m_network_manager = nullptr;
};
