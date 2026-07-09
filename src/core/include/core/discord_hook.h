#pragma once

#include "akashi/service.h"
#include "akashi_core_export.h"
#include "core/discord_message.h"

#include <QObject>

class QNetworkAccessManager;
class QNetworkReply;

namespace akashi {

// Posts Discord webhook messages and logs their outcomes. The hook is a
// dumb transport: which events become messages, and how they look, is the
// caller's business - the bundled akashi.discord plugin hooks modcalls
// and bans to it, and any plugin can post its own.
class AKASHI_CORE_EXPORT DiscordHook : public QObject, public IService
{
    Q_OBJECT

  public:
    explicit DiscordHook(QNetworkAccessManager *f_network, QObject *parent = nullptr);

    QString serviceId() const override;
    ServiceVersion serviceVersion() const override;

    void post(const DiscordMessage &f_message);
    void post(const DiscordMultipartMessage &f_message);

  private:
    void onDiscordResponse(QNetworkReply *f_reply);

    QNetworkAccessManager *m_network = nullptr;
};

} // namespace akashi
