#include "core/moderation_service.h"

#include "akashi/logging_categories.h"
#include "core/client_session.h"
#include "core/db_manager.h"
#include "core/permission_registry.h"
#include "core/server_context.h"

namespace akashi {

CoreModerationService::CoreModerationService(DBManager *f_db, ACLRolesHandler *f_roles, ServerContext *f_server) :
    m_db(f_db),
    m_roles(f_roles),
    m_server(f_server)
{}

static QList<BanHistoryEntry> toHistory(const QList<DBManager::BanInfo> &f_bans)
{
    QList<BanHistoryEntry> l_history;
    l_history.reserve(f_bans.size());
    for (const DBManager::BanInfo &l_ban : f_bans) {
        BanHistoryEntry l_entry;
        l_entry.ban_id = l_ban.id;
        l_entry.ipid = l_ban.ipid;
        l_entry.hdid = l_ban.hdid;
        l_entry.time = static_cast<qint64>(l_ban.time);
        l_entry.duration = l_ban.duration;
        l_entry.reason = l_ban.reason;
        l_entry.moderator = l_ban.moderator;
        l_history.append(l_entry);
    }
    return l_history;
}

QList<BanHistoryEntry> CoreModerationService::banHistory(const QString &f_ipid) const
{
    if (!m_db) {
        return {};
    }
    return toHistory(m_db->banInfo(QStringLiteral("ipid"), f_ipid));
}

QList<BanHistoryEntry> CoreModerationService::banHistoryByHwid(const QString &f_hwid) const
{
    if (!m_db) {
        return {};
    }
    return toHistory(m_db->banInfo(QStringLiteral("hdid"), f_hwid));
}

QList<SanctionEntry> CoreModerationService::activeSanctions(const QString &f_ipid) const
{
    if (!m_db) {
        return {};
    }
    QList<SanctionEntry> l_sanctions;
    const QList<DBManager::SanctionInfo> l_rows = m_db->sanctionsFor(f_ipid, QDateTime::currentSecsSinceEpoch());
    l_sanctions.reserve(l_rows.size());
    for (const DBManager::SanctionInfo &l_row : l_rows) {
        SanctionEntry l_entry;
        l_entry.ipid = l_row.ipid;
        l_entry.sanction = l_row.sanction;
        l_entry.issuer = l_row.moderator;
        l_entry.issued = l_row.issued;
        l_entry.expires = l_row.expires;
        l_entry.hwid = l_row.hwid;
        l_sanctions.append(l_entry);
    }
    return l_sanctions;
}

bool CoreModerationService::roleCanPerform(const QString &f_role_id, const QString &f_permission) const
{
    if (!m_roles || f_role_id.isEmpty()) {
        return false;
    }
    if (!m_roles->roleExists(f_role_id)) {
        return false;
    }
    return m_roles->roleById(f_role_id).canPerform(f_permission);
}

void CoreModerationService::applyTimedSanction(const QString &f_ipid, const QString &f_sanction_id, const QDateTime &f_until, const QString &f_issuer)
{
    if (!m_server) {
        qCWarning(akashiServer) << "Moderation service has no server; sanction not applied.";
        return;
    }
    m_server->applyTimedSanction(f_ipid, f_sanction_id, f_until, f_issuer);
}

void CoreModerationService::liftSanction(const QString &f_ipid, const QString &f_sanction_id)
{
    if (!m_server) {
        qCWarning(akashiServer) << "Moderation service has no server; sanction not lifted.";
        return;
    }
    const QList<akashi::ClientSession *> l_clients = m_server->removeSanction(f_ipid, f_sanction_id);
    for (akashi::ClientSession *l_client : l_clients) {
        l_client->sendServerMessage(QStringLiteral("Your \"%1\" sanction has been lifted.").arg(f_sanction_id));
    }
}

} // namespace akashi
