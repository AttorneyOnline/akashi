#pragma once

#include "akashi/authenticator.h"
#include "akashi_core_export.h"

#include <QFutureWatcher>
#include <QObject>
#include <QString>

#include <functional>

class DBManager;
class ServerSettings;

namespace akashi {

// The simple system: one shared modpass grants SUPER. moderator_name
// stays empty on purpose - the login log falls back to "Moderator", and
// /changepass relies on the empty name to refuse under simple auth.
class AKASHI_CORE_EXPORT PasswordAuthenticator : public Authenticator
{
  public:
    explicit PasswordAuthenticator(ServerSettings *f_settings);

    QString systemId() const override;
    void authenticate(const AuthRequest &f_request, std::function<void(const AuthOutcome &)> f_done) override;

  private:
    ServerSettings *m_settings;
};

// The advanced system: per-user accounts in the database. The PBKDF2
// hash runs off the main thread; the watcher is parented here so a
// pending login dies with the authenticator, never after it.
class AKASHI_CORE_EXPORT UsernameAuthenticator : public QObject, public Authenticator
{
    Q_OBJECT

  public:
    explicit UsernameAuthenticator(DBManager *f_database, QObject *f_parent = nullptr);

    QString systemId() const override;
    void authenticate(const AuthRequest &f_request, std::function<void(const AuthOutcome &)> f_done) override;

  private:
    // Finishes a login once the hash worker returns; authenticate binds
    // the attempt's particulars ahead of the watcher's finished signal.
    void onHashComputed(QFutureWatcher<QString> *f_watcher, const QString &f_username, const QString &f_password, const QString &f_stored_hash, const QString &f_acl_role, bool f_needs_rehash, std::function<void(const AuthOutcome &)> f_done);

    DBManager *m_database;
};

} // namespace akashi
