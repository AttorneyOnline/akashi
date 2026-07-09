#include "core/core_authenticators.h"

#include "core/crypto_helper.h"
#include "core/db_manager.h"
#include "core/permission_registry.h"
#include "core/server_settings.h"

#include <QtConcurrent/QtConcurrentRun>

#include <utility>

namespace akashi {

PasswordAuthenticator::PasswordAuthenticator(ServerSettings *f_settings) :
    m_settings(f_settings)
{}

QString PasswordAuthenticator::systemId() const
{
    return QStringLiteral("password");
}

void PasswordAuthenticator::authenticate(const AuthRequest &f_request, std::function<void(const AuthOutcome &)> f_done)
{
    const QString l_modpass = m_settings->modpass();
    // An empty modpass means the owner never set one; nothing may match it.
    if (!l_modpass.isEmpty() && f_request.args.value(0) == l_modpass) {
        AuthOutcome l_outcome;
        l_outcome.kind = AuthOutcome::Kind::Granted;
        l_outcome.acl_role_id = ACLRolesHandler::SUPER_ID;
        f_done(l_outcome);
        return;
    }

    AuthOutcome l_outcome;
    l_outcome.kind = AuthOutcome::Kind::Refused;
    l_outcome.message = QStringLiteral("Incorrect password.");
    f_done(l_outcome);
}

UsernameAuthenticator::UsernameAuthenticator(DBManager *f_database, QObject *f_parent) :
    QObject(f_parent),
    m_database(f_database)
{}

QString UsernameAuthenticator::systemId() const
{
    return QStringLiteral("username");
}

void UsernameAuthenticator::authenticate(const AuthRequest &f_request, std::function<void(const AuthOutcome &)> f_done)
{
    if (f_request.args.size() < 2) {
        AuthOutcome l_outcome;
        l_outcome.kind = AuthOutcome::Kind::Refused;
        l_outcome.message = QStringLiteral("You must specify a username and a password");
        f_done(l_outcome);
        return;
    }

    const QString l_username = f_request.args[0];
    const QString l_password = f_request.args[1];

    const auto l_creds = m_database->fetchCredentials(l_username);
    if (!l_creds) {
        AuthOutcome l_outcome;
        l_outcome.kind = AuthOutcome::Kind::Refused;
        l_outcome.message = QStringLiteral("Incorrect password.");
        // The attempted name still belongs in the login log.
        l_outcome.moderator_name = l_username;
        f_done(l_outcome);
        return;
    }

    const QString l_salt = l_creds->salt;
    const bool l_needs_rehash = QByteArray::fromHex(l_salt.toUtf8()).length() < CryptoHelper::pbkdf2_salt_len;

    QFuture<QString> l_future = QtConcurrent::run([l_salt, l_password]() {
        return CryptoHelper::hash_password(QByteArray::fromHex(l_salt.toUtf8()), l_password);
    });

    auto *l_watcher = new QFutureWatcher<QString>(this);
    connect(l_watcher, &QFutureWatcher<QString>::finished, this,
            std::bind_front(&UsernameAuthenticator::onHashComputed, this, l_watcher, l_username, l_password, l_creds->stored_hash, l_creds->acl_role, l_needs_rehash, std::move(f_done)));
    l_watcher->setFuture(l_future);
}

void UsernameAuthenticator::onHashComputed(QFutureWatcher<QString> *f_watcher, const QString &f_username, const QString &f_password, const QString &f_stored_hash, const QString &f_acl_role, bool f_needs_rehash, std::function<void(const AuthOutcome &)> f_done)
{
    f_watcher->deleteLater();

    const QString l_computed = f_watcher->result();
    if (CryptoHelper::constantTimeEquals(l_computed, f_stored_hash)) {
        AuthOutcome l_outcome;
        l_outcome.kind = AuthOutcome::Kind::Granted;
        l_outcome.acl_role_id = f_acl_role;
        l_outcome.moderator_name = f_username;
        l_outcome.message = QStringLiteral("Welcome, ") + f_username;
        f_done(l_outcome);

        // A legacy 8-byte salt rehashes to the current scheme, after the
        // outcome is delivered.
        if (f_needs_rehash) {
            m_database->updatePassword(f_username, f_password);
        }
        return;
    }

    AuthOutcome l_outcome;
    l_outcome.kind = AuthOutcome::Kind::Refused;
    l_outcome.message = QStringLiteral("Incorrect password.");
    l_outcome.moderator_name = f_username;
    f_done(l_outcome);
}

} // namespace akashi
