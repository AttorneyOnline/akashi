// AI-generated: written by Claude.
#include "core/authenticator_registry.h"

#include <QRegularExpression>
#include <QTest>

namespace tests {
namespace unittests {

using namespace akashi;

// A minimal system for the registry's bookkeeping tests.
class StubAuthenticator : public Authenticator
{
  public:
    explicit StubAuthenticator(const QString &f_id) :
        m_id(f_id) {}

    QString systemId() const override { return m_id; }

    void authenticate(const AuthRequest &, std::function<void(const AuthOutcome &)> f_done) override
    {
        f_done(AuthOutcome{});
    }

  private:
    QString m_id;
};

class tst_AuthenticatorRegistry : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void duplicateIdIsRefusedAndTheOldestWins();
    void unknownIdResolvesToNull();
    void ownerSweepRemovesExactlyThatOwner();
};

void tst_AuthenticatorRegistry::duplicateIdIsRefusedAndTheOldestWins()
{
    AuthenticatorRegistry l_registry;
    const std::shared_ptr<Authenticator> l_first = std::make_shared<StubAuthenticator>(QStringLiteral("oauth2"));
    l_registry.registerAuthenticator(l_first, QStringLiteral("plugin-one"));

    // The refusal names both the refused owner and the one that keeps the id.
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("plugin-two.*plugin-one")));
    l_registry.registerAuthenticator(std::make_shared<StubAuthenticator>(QStringLiteral("oauth2")), QStringLiteral("plugin-two"));

    QCOMPARE(l_registry.authenticatorFor(QStringLiteral("oauth2")), l_first);
}

void tst_AuthenticatorRegistry::unknownIdResolvesToNull()
{
    AuthenticatorRegistry l_registry;
    l_registry.registerAuthenticator(std::make_shared<StubAuthenticator>(QStringLiteral("password")), QStringLiteral("core"));

    QVERIFY(!l_registry.authenticatorFor(QStringLiteral("oauth2")));
    QVERIFY(!l_registry.authenticatorFor(QString()));
}

void tst_AuthenticatorRegistry::ownerSweepRemovesExactlyThatOwner()
{
    AuthenticatorRegistry l_registry;
    l_registry.registerAuthenticator(std::make_shared<StubAuthenticator>(QStringLiteral("password")), QStringLiteral("core"));
    l_registry.registerAuthenticator(std::make_shared<StubAuthenticator>(QStringLiteral("oauth2")), QStringLiteral("plugin-one"));
    l_registry.registerAuthenticator(std::make_shared<StubAuthenticator>(QStringLiteral("saml")), QStringLiteral("plugin-one"));

    l_registry.unregisterAll(QStringLiteral("plugin-one"));

    QVERIFY(!l_registry.authenticatorFor(QStringLiteral("oauth2")));
    QVERIFY(!l_registry.authenticatorFor(QStringLiteral("saml")));
    QVERIFY(l_registry.authenticatorFor(QStringLiteral("password")));
}

}
}

QTEST_GUILESS_MAIN(tests::unittests::tst_AuthenticatorRegistry)

#include "tst_authenticator_registry.moc"
