#pragma once

#include "akashi/moderation.h"
#include "akashi_core_export.h"

class DBManager;
class ServerContext;

namespace akashi {

class ACLRolesHandler;

// Backs the akashi.moderation service with the server's own database,
// role table and sanction machinery. Owned by ServerContext; the tests
// construct one over an in-memory database with no server behind it, in
// which case the write verbs refuse with a warning.
class AKASHI_CORE_EXPORT CoreModerationService : public ModerationService
{
  public:
    CoreModerationService(DBManager *f_db, ACLRolesHandler *f_roles, ServerContext *f_server);

    QList<BanHistoryEntry> banHistory(const QString &f_ipid) const override;
    QList<BanHistoryEntry> banHistoryByHwid(const QString &f_hwid) const override;
    QList<SanctionEntry> activeSanctions(const QString &f_ipid) const override;
    bool roleCanPerform(const QString &f_role_id, const QString &f_permission) const override;
    void applyTimedSanction(const QString &f_ipid, const QString &f_sanction_id, const QDateTime &f_until, const QString &f_issuer) override;
    void liftSanction(const QString &f_ipid, const QString &f_sanction_id) override;

  private:
    DBManager *m_db;
    ACLRolesHandler *m_roles;
    ServerContext *m_server;
};

} // namespace akashi
