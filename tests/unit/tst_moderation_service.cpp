// AI-generated: written by Claude.
#include "akashi/permissions.h"
#include "core/db_manager.h"
#include "core/moderation_service.h"
#include "core/permission_registry.h"

#include <QRegularExpression>
#include <QSqlDatabase>
#include <QTest>

namespace tests {
namespace unittests {

class tst_ModerationService : public QObject
{
    Q_OBJECT

  private Q_SLOTS:
    void banHistoryReadsTheBanTable();
    void activeSanctionsSkipExpiredRows();
    void roleGateAnswersLikeAHumanModeratorCheck();
    void writesRefuseWithoutAServer();
};

void tst_ModerationService::banHistoryReadsTheBanTable()
{
    QSqlDatabase l_database = QSqlDatabase::addDatabase("QSQLITE", "test_moderation_bans");
    l_database.setDatabaseName(":memory:");
    QVERIFY(l_database.open());
    DBManager l_manager(l_database);

    DBManager::BanInfo l_ban;
    l_ban.ipid = "abcd1234";
    l_ban.hdid = "hw-001";
    l_ban.time = 1000;
    l_ban.duration = 60;
    l_ban.reason = "spamming";
    l_ban.moderator = "mod";
    l_manager.addBan(l_ban);

    akashi::CoreModerationService l_service(&l_manager, nullptr, nullptr);

    const auto l_by_ipid = l_service.banHistory("abcd1234");
    QCOMPARE(l_by_ipid.size(), 1);
    QCOMPARE(l_by_ipid.first().reason, QStringLiteral("spamming"));
    QCOMPARE(l_by_ipid.first().time, 1000);
    QCOMPARE(l_by_ipid.first().duration, 60);
    QCOMPARE(l_by_ipid.first().moderator, QStringLiteral("mod"));

    const auto l_by_hwid = l_service.banHistoryByHwid("hw-001");
    QCOMPARE(l_by_hwid.size(), 1);
    QCOMPARE(l_by_hwid.first().ipid, QStringLiteral("abcd1234"));

    QVERIFY(l_service.banHistory("nobody").isEmpty());
}

void tst_ModerationService::activeSanctionsSkipExpiredRows()
{
    QSqlDatabase l_database = QSqlDatabase::addDatabase("QSQLITE", "test_moderation_sanctions");
    l_database.setDatabaseName(":memory:");
    QVERIFY(l_database.open());
    DBManager l_manager(l_database);

    const qint64 l_now = QDateTime::currentSecsSinceEpoch();
    l_manager.upsertSanction({"abcd1234", "muted", "modbot", l_now - 100, l_now + 600});
    l_manager.upsertSanction({"abcd1234", "gimped", "mod", l_now - 600, l_now - 100});

    akashi::CoreModerationService l_service(&l_manager, nullptr, nullptr);
    const auto l_active = l_service.activeSanctions("abcd1234");
    QCOMPARE(l_active.size(), 1);
    QCOMPARE(l_active.first().sanction, QStringLiteral("muted"));
    QCOMPARE(l_active.first().issuer, QStringLiteral("modbot"));
}

void tst_ModerationService::roleGateAnswersLikeAHumanModeratorCheck()
{
    akashi::ACLRolesHandler l_roles;
    akashi::ACLRole l_bot_role;
    l_bot_role.setPermission(akashi::permission::sanction_mute, true);
    QVERIFY(l_roles.insertRole("modbot", l_bot_role));

    akashi::CoreModerationService l_service(nullptr, &l_roles, nullptr);

    // The role holds exactly what the owner granted it.
    QVERIFY(l_service.roleCanPerform("modbot", akashi::permission::sanction_mute));
    QVERIFY(!l_service.roleCanPerform("modbot", akashi::permission::kick));

    // The built-in SUPER wildcard and unknown roles behave like they do
    // for humans: everything and nothing.
    QVERIFY(l_service.roleCanPerform("SUPER", akashi::permission::ban));
    QVERIFY(!l_service.roleCanPerform("nosuchrole", akashi::permission::sanction_mute));
    QVERIFY(!l_service.roleCanPerform("", akashi::permission::sanction_mute));
}

void tst_ModerationService::writesRefuseWithoutAServer()
{
    // A service without a server behind it must refuse loudly, not crash.
    akashi::CoreModerationService l_service(nullptr, nullptr, nullptr);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("no server")));
    l_service.applyTimedSanction("abcd1234", "muted", QDateTime::currentDateTime().addSecs(60), "modbot");
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("no server")));
    l_service.liftSanction("abcd1234", "muted");
}

} // namespace unittests
} // namespace tests

QTEST_GUILESS_MAIN(tests::unittests::tst_ModerationService)

#include "tst_moderation_service.moc"
