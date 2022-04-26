#include <QDebug>
#include <QFile>
#include <QTest>

#include <include/acl_roles_handler.h>

namespace tests {
namespace unittests {

/**
 * @brief Unit Tester class for ACL roles-related functions.
 */
class tst_ACLRolesHandler : public QObject
{
    Q_OBJECT

  public:
    ACLRolesHandler *m_handler;

  private slots:
    /**
     * @brief Initialises every tests with creating a new ACLRolesHandler.
     */
    void init();

    /**
     * @brief Tests the general state of read-only roles.
     */
    void checkReadOnlyRoles();

    /**
     * @brief Tests removal of read-only roles.
     */
    void removeReadOnlyRoles();

    /**
     * @brief Tests general modifications of read-only roles.
     */
    void replaceReadOnlyRoles();

    /**
     * @brief Tests general modifications of roles.
     */
    void modifyRoles();

    /**
     * @brief Tests clearance of roles.
     */
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
        QCOMPARE(m_handler->roleExists(ACLRolesHandler::NONE_ID), true);

        ACLRole l_role = m_handler->getRoleById(ACLRolesHandler::NONE_ID);
        // Checks every permissions
        QCOMPARE(l_role.checkPermission(ACLRole::NONE), true);
        QCOMPARE(l_role.checkPermission(ACLRole::KICK), false);
        QCOMPARE(l_role.checkPermission(ACLRole::BAN), false);
        QCOMPARE(l_role.checkPermission(ACLRole::BGLOCK), false);
        QCOMPARE(l_role.checkPermission(ACLRole::MODIFY_USERS), false);
        QCOMPARE(l_role.checkPermission(ACLRole::CM), false);
        QCOMPARE(l_role.checkPermission(ACLRole::GLOBAL_TIMER), false);
        QCOMPARE(l_role.checkPermission(ACLRole::EVI_MOD), false);
        QCOMPARE(l_role.checkPermission(ACLRole::MOTD), false);
        QCOMPARE(l_role.checkPermission(ACLRole::ANNOUNCE), false);
        QCOMPARE(l_role.checkPermission(ACLRole::MODCHAT), false);
        QCOMPARE(l_role.checkPermission(ACLRole::MUTE), false);
        QCOMPARE(l_role.checkPermission(ACLRole::UNCM), false);
        QCOMPARE(l_role.checkPermission(ACLRole::SAVETEST), false);
        QCOMPARE(l_role.checkPermission(ACLRole::FORCE_CHARSELECT), false);
        QCOMPARE(l_role.checkPermission(ACLRole::BYPASS_LOCKS), false);
        QCOMPARE(l_role.checkPermission(ACLRole::IGNORE_BGLIST), false);
        QCOMPARE(l_role.checkPermission(ACLRole::SEND_NOTICE), false);
        QCOMPARE(l_role.checkPermission(ACLRole::JUKEBOX), false);
        QCOMPARE(l_role.checkPermission(ACLRole::SUPER), false);
    }

    {
        const QString l_role_name = ACLRolesHandler::SUPER_ID;

        // Checks if the role exists
        QCOMPARE(m_handler->roleExists(l_role_name), true);

        ACLRole l_role = m_handler->getRoleById(l_role_name);
        // Checks every permissions
        QCOMPARE(l_role.checkPermission(ACLRole::NONE), true);
        QCOMPARE(l_role.checkPermission(ACLRole::KICK), true);
        QCOMPARE(l_role.checkPermission(ACLRole::BAN), true);
        QCOMPARE(l_role.checkPermission(ACLRole::BGLOCK), true);
        QCOMPARE(l_role.checkPermission(ACLRole::MODIFY_USERS), true);
        QCOMPARE(l_role.checkPermission(ACLRole::CM), true);
        QCOMPARE(l_role.checkPermission(ACLRole::GLOBAL_TIMER), true);
        QCOMPARE(l_role.checkPermission(ACLRole::EVI_MOD), true);
        QCOMPARE(l_role.checkPermission(ACLRole::MOTD), true);
        QCOMPARE(l_role.checkPermission(ACLRole::ANNOUNCE), true);
        QCOMPARE(l_role.checkPermission(ACLRole::MODCHAT), true);
        QCOMPARE(l_role.checkPermission(ACLRole::MUTE), true);
        QCOMPARE(l_role.checkPermission(ACLRole::UNCM), true);
        QCOMPARE(l_role.checkPermission(ACLRole::SAVETEST), true);
        QCOMPARE(l_role.checkPermission(ACLRole::FORCE_CHARSELECT), true);
        QCOMPARE(l_role.checkPermission(ACLRole::BYPASS_LOCKS), true);
        QCOMPARE(l_role.checkPermission(ACLRole::IGNORE_BGLIST), true);
        QCOMPARE(l_role.checkPermission(ACLRole::SEND_NOTICE), true);
        QCOMPARE(l_role.checkPermission(ACLRole::JUKEBOX), true);
        QCOMPARE(l_role.checkPermission(ACLRole::SUPER), true);
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

        const ACLRole l_role = m_handler->getRoleById(l_role_id);
        // Checks every permissions
        QCOMPARE(l_role.checkPermission(ACLRole::NONE), true);
        QCOMPARE(l_role.checkPermission(ACLRole::KICK), false);
        QCOMPARE(l_role.checkPermission(ACLRole::MODIFY_USERS), true);
        QCOMPARE(l_role.checkPermission(ACLRole::SUPER), false);

        // Removes the role.
        QCOMPARE(m_handler->removeRole(l_role_id), true);

        // Removes the role again. This should fail.
        QCOMPARE(m_handler->removeRole(l_role_id), false);

        // Checks if the role exists. This should fail.
        QCOMPARE(m_handler->roleExists(l_role_id), false);
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

#include "tst_unittest_acl_roles_handler.moc"
