// AI-generated: written by Claude.
#include "core/permission_registry.h"

#include <QDebug>
#include <QFile>
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
        QCOMPARE(l_role.canPerform(permission::mute), true);
        QCOMPARE(l_role.canPerform(permission::bypass_locks), true);
        QCOMPARE(l_role.canPerform(permission::ignore_background_list), true);
        // gamemaster is declared false in the file, so it must not load.
        QCOMPARE(l_role.canPerform(permission::gamemaster), false);
        QCOMPARE(l_role.canPerform(permission::modify_users), false);
        QCOMPARE(l_role.canPerform(permission::super), false);
    }
    {
        QString l_role_id = "supervisor";
        QCOMPARE(m_handler->roleExists(l_role_id), true);
        ACLRole l_role = m_handler->roleById(l_role_id);

        QCOMPARE(l_role.canPerform(permission::ban), true);
        QCOMPARE(l_role.canPerform(permission::kick), true);
        QCOMPARE(l_role.canPerform(permission::mute), true);
        QCOMPARE(l_role.canPerform(permission::chat_moderator), true);
        QCOMPARE(l_role.canPerform(permission::modify_users), true);
        QCOMPARE(l_role.canPerform(permission::lock_background), false);
        QCOMPARE(l_role.canPerform(permission::super), false);
    }
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
