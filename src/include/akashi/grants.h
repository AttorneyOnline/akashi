#pragma once

#include <QString>

// The grant model: a permission is allowed when some standing offer
// grants it - the server's, a floor's, an area's, a role's, or a
// personal one. Grants only ever add; the single subtraction in the
// whole model is the sanction mask, applied after the union.
namespace akashi {

// Where a grant stands. Server-scope grants survive everything an
// in-game actor can do; floor and area grants live and die with their
// object.
enum class GrantScope
{
    Server,
    Floor,
    Area,
};

// Who a grant is offered to. Everyone means every joined session - the
// baseline a server offers people for existing. Participants is the
// narrower everyone that excludes spectators.
enum class AudienceKind
{
    Everyone,
    Role,
    Person,
    Participants,
};

struct Audience
{
    AudienceKind kind = AudienceKind::Everyone;
    QString role_id;    // Role: the roles-file id, matched case-insensitively.
    QString person_key; // Person: the IPID - the identity sanctions and bans already trust.

    static Audience everyone() { return {AudienceKind::Everyone, {}, {}}; }
    static Audience participants() { return {AudienceKind::Participants, {}, {}}; }
    static Audience role(const QString &f_role_id) { return {AudienceKind::Role, f_role_id, {}}; }
    static Audience person(const QString &f_ipid) { return {AudienceKind::Person, {}, f_ipid}; }

    bool operator==(const Audience &) const = default;
};

// One standing offer of one permission. The owner tag is provenance:
// stamped by the host from the caller's identity, never caller-supplied,
// and removal requires authority over the source.
struct Grant
{
    QString permission; // validated against the permission catalog at every sink
    Audience audience;
    GrantScope scope = GrantScope::Server;
    QString owner; // "core", "config", "roles", "plugin:<id>", ...

    bool operator==(const Grant &) const = default;
};

} // namespace akashi
