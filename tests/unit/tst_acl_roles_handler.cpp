// AI-generated: written by Claude.
#include "core/permission_registry.h"

#include <QDebug>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

using akashi::ACLRole;
using akashi::ACLRolesHandler;
namespace permission = akashi::permission;

namespace tests {
namespace unittests {

class tst_ACLRolesHandler : public QObject
{
    Q_OBJECT

  public:
    ACLRolesHandler *m_handler;

  private Q_SLOTS:
    void init();
    void cleanup();

    void checkReadOnlyRoles();

    void removeReadOnlyRoles();

    void replaceReadOnlyRoles();

    void modifyRoles();

    void loadRolesFromIni();

    void groupsExpandThroughRoles();

    void everyoneSectionDefinesTheBaseline();

    void clearAllRoles();
};

void tst_ACLRolesHandler::init()
{
    m_handler = new ACLRolesHandler;
}

void tst_ACLRolesHandler::cleanup()
{
    delete m_handler;
}

void tst_ACLRolesHandler::checkReadOnlyRoles()
{
    {
        // Checks if the role exists
        QCOMPARE(m_handler->roleExists(ACLRolesHandler::NONE_ID), true);

        ACLRole l_role = m_handler->roleById(ACLRolesHandler::NONE_ID);
        // Holds nothing, but empty and "none" checks always pass.
        QCOMPARE(l_role.permissions().isEmpty(), true);
        QCOMPARE(l_role.canPerform(permission::none), true);
        QCOMPARE(l_role.canPerform(QString()), true);
        QCOMPARE(l_role.canPerform(permission::kick), false);
        QCOMPARE(l_role.canPerform(permission::ban), false);
        QCOMPARE(l_role.canPerform(permission::super), false);
    }

    {
        // Checks if the role exists
        QCOMPARE(m_handler->roleExists(ACLRolesHandler::SUPER_ID), true);

        ACLRole l_role = m_handler->roleById(ACLRolesHandler::SUPER_ID);
        // The super wildcard grants every permission, known or not.
        QCOMPARE(l_role.canPerform(permission::none), true);
        QCOMPARE(l_role.canPerform(permission::kick), true);
        QCOMPARE(l_role.canPerform(permission::ban), true);
        QCOMPARE(l_role.canPerform(permission::modify_users), true);
        QCOMPARE(l_role.canPerform(permission::super), true);
        QCOMPARE(l_role.canPerform(QStringLiteral("plugin.custom_permission")), true);
    }
}

void tst_ACLRolesHandler::removeReadOnlyRoles()
{
    { // SUPER role
        // Removes the role. This should fail.
        QCOMPARE(m_handler->removeRole(ACLRolesHandler::SUPER_ID), false);

        // Checks if the role exists.
        QCOMPARE(m_handler->roleExists(ACLRolesHandler::SUPER_ID), true);
    }
}

void tst_ACLRolesHandler::replaceReadOnlyRoles()
{
    {
        // Attempts to overwrite a read-only role. This should fail.
        QCOMPARE(m_handler->insertRole(ACLRolesHandler::NONE_ID, ACLRole()), false);
    }
}

void tst_ACLRolesHandler::modifyRoles()
{
    {
        const QString l_role_id = "new_role";

        // Checks if a the role exists. This should fail.
        QCOMPARE(m_handler->roleExists(l_role_id), false);

        // Inserts a role.
        QCOMPARE(m_handler->insertRole(l_role_id, ACLRole({permission::kick})), true);

        // Inserts a role again, overwriting the first.
        QCOMPARE(m_handler->insertRole(l_role_id, ACLRole({permission::modify_users})), true);

        // Checks if the role exists.
        QCOMPARE(m_handler->roleExists(l_role_id), true);

        const ACLRole l_role = m_handler->roleById(l_role_id);
        // Only the overwrite's permission set remains.
        QCOMPARE(l_role.canPerform(permission::none), true);
        QCOMPARE(l_role.canPerform(permission::kick), false);
        QCOMPARE(l_role.canPerform(permission::modify_users), true);
        QCOMPARE(l_role.canPerform(permission::super), false);

        // Removes the role.
        QCOMPARE(m_handler->removeRole(l_role_id), true);

        // Removes the role again. This should fail.
        QCOMPARE(m_handler->removeRole(l_role_id), false);

        // Checks if the role exists. This should fail.
        QCOMPARE(m_handler->roleExists(l_role_id), false);
    }
}

void tst_ACLRolesHandler::loadRolesFromIni()
{
    {
        qDebug() << QDir::currentPath();
        QFile config_file("config/acl_roles.ini");
        QCOMPARE(config_file.exists(), true);
    }
    {
        m_handler->loadFile("config/acl_roles.ini");
    }
    {
        QString l_role_id = "moderator";
        QCOMPARE(m_handler->roleExists(l_role_id), true);
        ACLRole l_role = m_handler->roleById(l_role_id);

        QCOMPARE(l_role.canPerform(permission::none), true);
        QCOMPARE(l_role.canPerform(permission::kick), true);
        QCOMPARE(l_role.canPerform(permission::ban), true);
        QCOMPARE(l_role.canPerform(permission::lock_background), true);
        QCOMPARE(l_role.canPerform(permission::chat_moderator), true);
        // The legacy names load through the alias table: "mute" expands to
        // the per-sanction set, "bypass_locks" to area.enter - which is
        // what the constants carry now.
        QCOMPARE(l_role.canPerform(permission::sanction_mute), true);
        QCOMPARE(l_role.canPerform(permission::sanction_charcurse), true);
        QCOMPARE(l_role.canPerform(permission::area_enter), true);
        QCOMPARE(l_role.canPerform(permission::ignore_background_list), true);
        // gamemaster is declared false in the file, so it must not load.
        QCOMPARE(l_role.canPerform(permission::area_cm), false);
        QCOMPARE(l_role.canPerform(permission::modify_users), false);
        QCOMPARE(l_role.canPerform(permission::super), false);
    }
    {
        QString l_role_id = "supervisor";
        QCOMPARE(m_handler->roleExists(l_role_id), true);
        ACLRole l_role = m_handler->roleById(l_role_id);

        QCOMPARE(l_role.canPerform(permission::ban), true);
        QCOMPARE(l_role.canPerform(permission::kick), true);
        QCOMPARE(l_role.canPerform(permission::sanction_mute), true);
        QCOMPARE(l_role.canPerform(permission::chat_moderator), true);
        QCOMPARE(l_role.canPerform(permission::modify_users), true);
        QCOMPARE(l_role.canPerform(permission::lock_background), false);
        QCOMPARE(l_role.canPerform(permission::super), false);
    }
}

void tst_ACLRolesHandler::groupsExpandThroughRoles()
{
    QTemporaryDir l_dir;
    QVERIFY(l_dir.isValid());
    const QString l_path = l_dir.filePath("permissions.json");
    {
        QFile l_file(l_path);
        QVERIFY(l_file.open(QIODevice::WriteOnly | QIODevice::Text));
        l_file.write(R"({
            "groups": {
                "@chat_tools": ["sanction.mute"],
                "@moderation": ["kick", "ban", "@chat_tools"],
                "@loop_a": ["@loop_b", "kick"],
                "@loop_b": ["@loop_a", "ban"]
            },
            "helper": { "@chat_tools": "true" },
            "mod": { "@moderation": "true", "motd": "true" },
            "typo": { "@moderaton": "true", "kick": "true" },
            "loopy": { "@loop_a": "true" }
        })");
    }

    // The two group cycles are cut with a warning each, and the typo'd
    // reference fails loudly instead of loading as a mystery permission.
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("references itself through")));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("references itself through")));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("references unknown group")));
    QVERIFY(m_handler->loadFile(l_path));

    // Group lookups take the reference with or without the sigil.
    QVERIFY(m_handler->groupExists(QStringLiteral("@moderation")));
    QVERIFY(m_handler->groupExists(QStringLiteral("Moderation")));
    QVERIFY(!m_handler->groupExists(QStringLiteral("@moderaton")));
    QCOMPARE(m_handler->groupMembers(QStringLiteral("@chat_tools")), QStringList({QStringLiteral("sanction.mute")}));

    // A nested reference flattens to its leaves.
    const ACLRole l_mod = m_handler->roleById(QStringLiteral("mod"));
    QVERIFY(l_mod.canPerform(permission::kick));
    QVERIFY(l_mod.canPerform(permission::ban));
    QVERIFY(l_mod.canPerform(permission::sanction_mute));
    QVERIFY(l_mod.canPerform(permission::motd));
    QVERIFY(!l_mod.canPerform(permission::super));

    // A group hands over exactly its keyring, nothing more.
    const ACLRole l_helper = m_handler->roleById(QStringLiteral("helper"));
    QVERIFY(l_helper.canPerform(permission::sanction_mute));
    QVERIFY(!l_helper.canPerform(permission::kick));

    // The typo'd role keeps its direct grant and only loses the unknown
    // reference.
    const ACLRole l_typo = m_handler->roleById(QStringLiteral("typo"));
    QVERIFY(l_typo.canPerform(permission::kick));
    QVERIFY(!l_typo.canPerform(permission::ban));

    // A cut cycle still delivers the non-cyclic members.
    const ACLRole l_loopy = m_handler->roleById(QStringLiteral("loopy"));
    QVERIFY(l_loopy.canPerform(permission::kick));
    QVERIFY(l_loopy.canPerform(permission::ban));
}

void tst_ACLRolesHandler::everyoneSectionDefinesTheBaseline()
{
    QTemporaryDir l_dir;
    QVERIFY(l_dir.isValid());

    // A file without the section leaves the baseline undeclared - the
    // caller applies the stock default then.
    const QString l_bare_path = l_dir.filePath("bare.json");
    {
        QFile l_file(l_bare_path);
        QVERIFY(l_file.open(QIODevice::WriteOnly | QIODevice::Text));
        l_file.write(R"({ "mod": { "kick": "true" } })");
    }
    QVERIFY(m_handler->loadFile(l_bare_path));
    QVERIFY(!m_handler->everyoneBaseline().has_value());

    // The section reads like a role body - @groups expand, legacy names
    // translate - but lands as the baseline, never as a wearable role.
    const QString l_path = l_dir.filePath("permissions.json");
    {
        QFile l_file(l_path);
        QVERIFY(l_file.open(QIODevice::WriteOnly | QIODevice::Text));
        l_file.write(R"({
            "groups": { "@default": ["music.play", "roleplay.roll"] },
            "everyone": { "@default": "true", "jukebox": "true", "casing.doc": "false" },
            "mod": { "kick": "true" }
        })");
    }
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("by its legacy name")));
    QVERIFY(m_handler->loadFile(l_path));

    QVERIFY(m_handler->everyoneBaseline().has_value());
    QStringList l_baseline = m_handler->everyoneBaseline().value();
    l_baseline.sort();
    QCOMPARE(l_baseline, QStringList({permission::music_jukebox, permission::music_play, permission::roleplay_roll}));
    QVERIFY(!m_handler->roleExists(QStringLiteral("everyone")));

    // The section survives a save: user management writing roles back must
    // never silently delete the baseline.
    const QString l_saved_path = l_dir.filePath("saved.json");
    QVERIFY(m_handler->saveFile(l_saved_path));
    ACLRolesHandler l_reloaded;
    QVERIFY(l_reloaded.loadFile(l_saved_path));
    QVERIFY(l_reloaded.everyoneBaseline().has_value());
    QStringList l_survivors = l_reloaded.everyoneBaseline().value();
    l_survivors.sort();
    QCOMPARE(l_survivors, QStringList({permission::music_jukebox, permission::music_play, permission::roleplay_roll}));
    QVERIFY(l_reloaded.roleExists(QStringLiteral("mod")));
}

void tst_ACLRolesHandler::clearAllRoles()
{
    {
        const QString l_role_id = "new_role";

        // Inserts a role.
        QCOMPARE(m_handler->insertRole(l_role_id, ACLRole({permission::kick})), true);

        m_handler->clearRoles();
        // Checks if a the role exists. This should fail.
        QCOMPARE(m_handler->roleExists(l_role_id), false);
    }
}

}
}

QTEST_APPLESS_MAIN(tests::unittests::tst_ACLRolesHandler)

#include "tst_acl_roles_handler.moc"
