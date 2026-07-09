#include "core/authenticator_registry.h"

#include "akashi/logging_categories.h"
#include "akashi/thread_assert.h"

#include <QDebug>

#include <utility>

namespace akashi {

AuthenticatorRegistry::AuthenticatorRegistry() :
    m_owner_thread(QThread::currentThread())
{}

QString AuthenticatorRegistry::serviceId() const
{
    return QStringLiteral("akashi.auth");
}

ServiceVersion AuthenticatorRegistry::serviceVersion() const
{
    return {1, 0, 0};
}

void AuthenticatorRegistry::registerAuthenticator(std::shared_ptr<Authenticator> f_authenticator, const QString &f_owner)
{
    AKASHI_ASSERT_OWNER_THREAD();
    if (!f_authenticator) {
        return;
    }
    // A system id registers once; a duplicate would silently swap the
    // system the config names while the id stays in the first owner's
    // unregisterAll sweep.
    const QString l_id = f_authenticator->systemId();
    for (const Entry &l_existing : std::as_const(m_entries)) {
        if (l_existing.authenticator->systemId() == l_id) {
            qCWarning(akashiServer) << "Refused authenticator" << l_id << "from" << f_owner
                                    << "- the id is already registered by" << l_existing.owner;
            return;
        }
    }
    m_entries.append({std::move(f_authenticator), f_owner});
}

void AuthenticatorRegistry::unregisterAll(const QString &f_owner)
{
    AKASHI_ASSERT_OWNER_THREAD();
    m_entries.removeIf([&](const Entry &e) { return e.owner == f_owner; });
}

std::shared_ptr<Authenticator> AuthenticatorRegistry::authenticatorFor(const QString &f_id) const
{
    AKASHI_ASSERT_OWNER_THREAD();
    for (const Entry &l_entry : m_entries) {
        if (l_entry.authenticator->systemId() == f_id) {
            return l_entry.authenticator;
        }
    }
    return nullptr;
}

} // namespace akashi
