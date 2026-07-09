#pragma once

#include "akashi/authenticator.h"
#include "akashi/service.h"
#include "akashi_core_export.h"

#include <QList>
#include <QString>

#include <memory>

class QThread;

namespace akashi {

// Every authentication system the server could run, keyed by the stable
// system id. Exactly one becomes active at launch; the registry stays
// open only so plugins can offer their systems before resolution.
class AKASHI_CORE_EXPORT AuthenticatorRegistry : public IService
{
  public:
    AuthenticatorRegistry();

    QString serviceId() const override;
    ServiceVersion serviceVersion() const override;

    // A system id registers once - a second registration is refused with
    // a warning naming both owners, and the first owner keeps the id.
    void registerAuthenticator(std::shared_ptr<Authenticator> f_authenticator, const QString &f_owner = {});

    void unregisterAll(const QString &f_owner);

    // The system carrying the id, or null when nothing does.
    std::shared_ptr<Authenticator> authenticatorFor(const QString &f_id) const;

  private:
    struct Entry
    {
        std::shared_ptr<Authenticator> authenticator;
        QString owner;
    };

    QList<Entry> m_entries;
    QThread *m_owner_thread;
};

} // namespace akashi
