#pragma once

#include <QString>
#include <QStringList>

#include <functional>

namespace akashi {

// What a login attempt carries to the active authentication system.
struct AuthRequest
{
    int client_id = -1;
    QString ipid;
    QStringList args;
};

// What the system decided. Granted carries the role, the moderator name
// and an optional welcome; Refused carries the reason; Challenge carries
// fields the server relays to the client verbatim for another round.
struct AuthOutcome
{
    enum class Kind
    {
        Granted,
        Refused,
        Challenge,
    };

    Kind kind = Kind::Refused;
    QString acl_role_id;
    QString moderator_name;
    QString message;
    QStringList challenge;
};

// One authentication system. The server resolves exactly one at launch,
// named by the config's auth setting, and never switches while running.
class Authenticator
{
  public:
    virtual ~Authenticator() = default;

    // The STABLE identifier the config names and the wire speaks. It must
    // never depend on configuration or runtime state.
    virtual QString systemId() const = 0;

    // Decides one login attempt. f_done fires exactly once, on the main
    // thread, synchronously or later.
    virtual void authenticate(const AuthRequest &f_request, std::function<void(const AuthOutcome &)> f_done) = 0;
};

// Everything below is appended: this SDK header is append-only.

} // namespace akashi
