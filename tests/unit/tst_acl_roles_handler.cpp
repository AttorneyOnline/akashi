// AI-generated: written by Claude.
#include "acl_roles_handler.h"

#include <QDebug>
#include <QFile>
#include <QTest>

namespace tests {
namespace unittests {

class tst_ACLRolesHandler : public QObject
{
    Q_OBJECT

  public:
    ACLRolesHandler *m_handler;

  private Q_SLOTS:
    void init();

    void checkReadOnlyRoles();

    void removeReadOnlyRoles();

    void replaceReadOnlyRoles();

    void modifyRoles();

    void loadRolesFromIni();

    void clearAllRoles();
};

void tst_ACLRolesHandler::init()
{
    m_handler = new ACLRolesHandler;
}

void tst_ACLRolesHandler::checkReadOnlyRoles()
{
    {
        const QString l_role_name = ACLRolesHandler::NONE_ID;

        // Checks if the role exists
        QCOMPARE(m_handler->roleExists(l_role_name), true);

        ACLRole l_role = m_handler->roleById(ACLRolesHandler::NONE_ID);
        // Checks every permissions
        QCOMPARE(l_role.canPerform(ACLRole::NONE), true);
        QCOMPARE(l_role.canPerform(ACLRole::KICK), false);
        QCOMPARE(l_role.canPerform(ACLRole::BAN), false);
        QCOMPARE(l_role.canPerform(ACLRole::BGLOCK), false);
        QCOMPARE(l_role.canPerform(ACLRole::MODIFY_USERS), false);
        QCOMPARE(l_role.canPerform(ACLRole::CM), false);
        QCOMPARE(l_role.canPerform(ACLRole::GLOBAL_TIMER), false);
        QCOMPARE(l_role.canPerform(ACLRole::EVI_MOD), false);
        QCOMPARE(l_role.canPerform(ACLRole::MOTD), false);
        QCOMPARE(l_role.canPerform(ACLRole::ANNOUNCE), false);
        QCOMPARE(l_role.canPerform(ACLRole::MODCHAT), false);
        QCOMPARE(l_role.canPerform(ACLRole::MUTE), false);
        QCOMPARE(l_role.canPerform(ACLRole::UNCM), false);
        QCOMPARE(l_role.canPerform(ACLRole::SAVETEST), false);
        QCOMPARE(l_role.canPerform(ACLRole::FORCE_CHARSELECT), false);
        QCOMPARE(l_role.canPerform(ACLRole::BYPASS_LOCKS), false);
        QCOMPARE(l_role.canPerform(ACLRole::IGNORE_BGLIST), false);
        QCOMPARE(l_role.canPerform(ACLRole::SEND_NOTICE), false);
        QCOMPARE(l_role.canPerform(ACLRole::JUKEBOX), false);
        QCOMPARE(l_role.canPerform(ACLRole::SUPER), false);
    }

    {
        const QString l_role_name = ACLRolesHandler::SUPER_ID;

        // Checks if the role exists
        QCOMPARE(m_handler->roleExists(l_role_name), true);

        ACLRole l_role = m_handler->roleById(l_role_name);
        // Checks every permissions
        QCOMPARE(l_role.canPerform(ACLRole::NONE), true);
        QCOMPARE(l_role.canPerform(ACLRole::KICK), true);
        QCOMPARE(l_role.canPerform(ACLRole::BAN), true);
        QCOMPARE(l_role.canPerform(ACLRole::BGLOCK), true);
        QCOMPARE(l_role.canPerform(ACLRole::MODIFY_USERS), true);
        QCOMPARE(l_role.canPerform(ACLRole::CM), true);
        QCOMPARE(l_role.canPerform(ACLRole::GLOBAL_TIMER), true);
        QCOMPARE(l_role.canPerform(ACLRole::EVI_MOD), true);
        QCOMPARE(l_role.canPerform(ACLRole::MOTD), true);
        QCOMPARE(l_role.canPerform(ACLRole::ANNOUNCE), true);
        QCOMPARE(l_role.canPerform(ACLRole::MODCHAT), true);
        QCOMPARE(l_role.canPerform(ACLRole::MUTE), true);
        QCOMPARE(l_role.canPerform(ACLRole::UNCM), true);
        QCOMPARE(l_role.canPerform(ACLRole::SAVETEST), true);
        QCOMPARE(l_role.canPerform(ACLRole::FORCE_CHARSELECT), true);
        QCOMPARE(l_role.canPerform(ACLRole::BYPASS_LOCKS), true);
        QCOMPARE(l_role.canPerform(ACLRole::IGNORE_BGLIST), true);
        QCOMPARE(l_role.canPerform(ACLRole::SEND_NOTICE), true);
        QCOMPARE(l_role.canPerform(ACLRole::JUKEBOX), true);
        QCOMPARE(l_role.canPerform(ACLRole::SUPER), true);
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
        QCOMPARE(m_handler->insertRole(ACLRolesHandler::NONE_ID, ACLRole(ACLRole::NONE)), false);
    }
}

void tst_ACLRolesHandler::modifyRoles()
{
    {
        const QString l_role_id = "new_role";

        // Checks if a the role exists. This should fail.
        QCOMPARE(m_handler->roleExists(l_role_id), false);

        // Inserts a role.
        QCOMPARE(m_handler->insertRole(l_role_id, ACLRole(ACLRole::KICK)), true);

        // Inserts a role again.
        QCOMPARE(m_handler->insertRole(l_role_id, ACLRole(ACLRole::MODIFY_USERS)), true);

        // Checks if the role exists.
        QCOMPARE(m_handler->roleExists(l_role_id), true);

        const ACLRole l_role = m_handler->roleById(l_role_id);
        // Checks every permissions
        QCOMPARE(l_role.canPerform(ACLRole::NONE), true);
        QCOMPARE(l_role.canPerform(ACLRole::KICK), false);
        QCOMPARE(l_role.canPerform(ACLRole::MODIFY_USERS), true);
        QCOMPARE(l_role.canPerform(ACLRole::SUPER), false);

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

        QCOMPARE(l_role.canPerform(ACLRole::NONE), true);
        QCOMPARE(l_role.canPerform(ACLRole::KICK), true);
        QCOMPARE(l_role.canPerform(ACLRole::BAN), true);
        QCOMPARE(l_role.canPerform(ACLRole::BGLOCK), true);
        QCOMPARE(l_role.canPerform(ACLRole::MODIFY_USERS), false);
        QCOMPARE(l_role.canPerform(ACLRole::CM), false);
        QCOMPARE(l_role.canPerform(ACLRole::GLOBAL_TIMER), false);
        QCOMPARE(l_role.canPerform(ACLRole::EVI_MOD), false);
        QCOMPARE(l_role.canPerform(ACLRole::MOTD), false);
        QCOMPARE(l_role.canPerform(ACLRole::ANNOUNCE), false);
        QCOMPARE(l_role.canPerform(ACLRole::MODCHAT), true);
        QCOMPARE(l_role.canPerform(ACLRole::MUTE), true);
        QCOMPARE(l_role.canPerform(ACLRole::UNCM), false);
        QCOMPARE(l_role.canPerform(ACLRole::SAVETEST), false);
        QCOMPARE(l_role.canPerform(ACLRole::FORCE_CHARSELECT), false);
        QCOMPARE(l_role.canPerform(ACLRole::BYPASS_LOCKS), true);
        QCOMPARE(l_role.canPerform(ACLRole::IGNORE_BGLIST), true);
        QCOMPARE(l_role.canPerform(ACLRole::SEND_NOTICE), false);
        QCOMPARE(l_role.canPerform(ACLRole::JUKEBOX), false);
        QCOMPARE(l_role.canPerform(ACLRole::SUPER), false);
    }
}

void tst_ACLRolesHandler::clearAllRoles()
{
    {
        const QString l_role_id = "new_role";

        // Inserts a role.
        QCOMPARE(m_handler->insertRole(l_role_id, ACLRole(ACLRole::KICK)), true);

        m_handler->clearRoles();
        // Checks if a the role exists. This should fail.
        QCOMPARE(m_handler->roleExists(l_role_id), false);
    }
}

}
}

QTEST_APPLESS_MAIN(tests::unittests::tst_ACLRolesHandler)

#include "tst_acl_roles_handler.moc"
