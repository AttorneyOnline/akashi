// AI-generated: written by Claude.
#include "core/core_authenticators.h"
#include "core/crypto_helper.h"
#include "core/db_manager.h"

#include <QDeadlineTimer>
#include <QSqlDatabase>
#include <QTest>

namespace tests {
namespace unittests {

using namespace akashi;

class tst_UsernameAuthenticator : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void createdUserAuthenticates();
    void wrongPasswordIsRefused();
    void unknownUserIsRefused();

  private:
    // A real DBManager over an in-memory database, like tst_dbmanager's.
    QSqlDatabase openDatabase(const QString &f_connection);
    // Runs one attempt and spins the loop until the outcome lands; the
    // hash worker answers through the event loop.
    AuthOutcome authenticate(UsernameAuthenticator &f_authenticator, const QStringList &f_args);
};

QSqlDatabase tst_UsernameAuthenticator::openDatabase(const QString &f_connection)
{
    QSqlDatabase l_database = QSqlDatabase::addDatabase("QSQLITE", f_connection);
    l_database.setDatabaseName(":memory:");
    return l_database;
}

AuthOutcome tst_UsernameAuthenticator::authenticate(UsernameAuthenticator &f_authenticator, const QStringList &f_args)
{
    AuthOutcome l_outcome;
    bool l_done = false;
    f_authenticator.authenticate({7, QStringLiteral("1234"), f_args}, [&l_outcome, &l_done](const AuthOutcome &f_outcome) {
        l_outcome = f_outcome;
        l_done = true;
    });

    QDeadlineTimer l_deadline(15000);
    while (!l_done && !l_deadline.hasExpired()) {
        QTest::qWait(10);
    }
    return l_outcome;
}

void tst_UsernameAuthenticator::createdUserAuthenticates()
{
    QSqlDatabase l_database = openDatabase("auth_test_granted");
    QVERIFY(l_database.open());
    DBManager l_manager(l_database);
    QVERIFY(l_manager.createUser("mod", CryptoHelper::randbytes(16), "GoodPass1!", "MODERATOR"));

    UsernameAuthenticator l_authenticator(&l_manager);
    const AuthOutcome l_outcome = authenticate(l_authenticator, {"mod", "GoodPass1!"});
    QCOMPARE(l_outcome.kind, AuthOutcome::Kind::Granted);
    QCOMPARE(l_outcome.acl_role_id, QStringLiteral("MODERATOR"));
    QCOMPARE(l_outcome.moderator_name, QStringLiteral("mod"));
    QCOMPARE(l_outcome.message, QStringLiteral("Welcome, mod"));
}

void tst_UsernameAuthenticator::wrongPasswordIsRefused()
{
    QSqlDatabase l_database = openDatabase("auth_test_wrong_password");
    QVERIFY(l_database.open());
    DBManager l_manager(l_database);
    QVERIFY(l_manager.createUser("mod", CryptoHelper::randbytes(16), "GoodPass1!", "MODERATOR"));

    UsernameAuthenticator l_authenticator(&l_manager);
    const AuthOutcome l_outcome = authenticate(l_authenticator, {"mod", "WrongPass1!"});
    QCOMPARE(l_outcome.kind, AuthOutcome::Kind::Refused);
    QCOMPARE(l_outcome.message, QStringLiteral("Incorrect password."));
    // The attempted name still reaches the login log.
    QCOMPARE(l_outcome.moderator_name, QStringLiteral("mod"));
}

void tst_UsernameAuthenticator::unknownUserIsRefused()
{
    QSqlDatabase l_database = openDatabase("auth_test_unknown_user");
    QVERIFY(l_database.open());
    DBManager l_manager(l_database);

    UsernameAuthenticator l_authenticator(&l_manager);
    const AuthOutcome l_outcome = authenticate(l_authenticator, {"ghost", "GoodPass1!"});
    QCOMPARE(l_outcome.kind, AuthOutcome::Kind::Refused);
    QCOMPARE(l_outcome.message, QStringLiteral("Incorrect password."));
    QCOMPARE(l_outcome.moderator_name, QStringLiteral("ghost"));
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_UsernameAuthenticator)

#include "tst_username_authenticator.moc"
