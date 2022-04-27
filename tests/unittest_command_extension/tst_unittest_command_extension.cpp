#include <QDebug>
#include <QRegularExpression>
#include <QTest>

#include <include/command_extension.h>

namespace tests {
namespace unittests {

/**
 * @brief Unit Tester class for ACL roles-related functions.
 */
class tst_CommandExtension : public QObject
{
    Q_OBJECT

  public:
    typedef QVector<ACLRole::Permission> PermVector;

    CommandExtension m_extension;

  private slots:
    /**
     * @brief Initialises every tests
     */
    void init();

    /**
     * @brief The data function of checkCommandName
     */
    void checkCommandName_data();

    /**
     * @brief Tests various command names
     */
    void checkCommandName();

    /**
     * @brief The data function of checkAliases
     */
    void checkAliases_data();

    /**
     * @brief Tests various aliases
     */
    void checkAliases();

    /**
     * @brief The data function of checkAlias
     */
    void checkAlias_data();

    /**
     * @brief checkAlias
     */
    void checkAlias();

    /**
     * @brief The data function of checkPermission
     */
    void checkPermission_data();

    /**
     * @brief Tests various permission scenarios
     */
    void checkPermission();

    /**
     * @brief The data function of setPermissionsByCaption
     */
    void setPermissionsByCaption_data();

    /**
     * @brief Tests the role caption conversion
     */
    void setPermissionsByCaption();
};

void tst_CommandExtension::init()
{
    m_extension = CommandExtension();
}

void tst_CommandExtension::checkCommandName_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QString>("expected_name");
    QTest::addColumn<bool>("expected_result");

    QTest::newRow("Identical name") << "extension"
                                    << "extension" << true;
    QTest::newRow("Different name") << "different"
                                    << "extension" << false;
    QTest::newRow("No name") << QString{}
                             << "extension" << false;
}

void tst_CommandExtension::checkCommandName()
{
    QFETCH(QString, name);
    QFETCH(QString, expected_name);
    QFETCH(bool, expected_result);

    {
        CommandExtension l_extension(name);
        QCOMPARE(l_extension.getCommandName() == expected_name, expected_result);
    }

    {
        CommandExtension l_extension;
        l_extension.setCommandName(name);
        QCOMPARE(l_extension.getCommandName() == expected_name, expected_result);
    }
}

void tst_CommandExtension::checkAliases_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QStringList>("aliases");
    QTest::addColumn<QStringList>("expected_aliases");
    QTest::addColumn<bool>("expected_result");

    QTest::newRow("Identical aliases") << "extension" << QStringList{"ext", "extended"} << QStringList{"ext", "extended"} << true;
    QTest::newRow("Different aliases") << "extension" << QStringList{"ext", "extended"} << QStringList{"will", "not", "be", "valid"} << false;
}

void tst_CommandExtension::checkAliases()
{
    QFETCH(QString, name);
    QFETCH(QStringList, aliases);
    QFETCH(QStringList, expected_aliases);
    QFETCH(bool, expected_result);

    {
        CommandExtension l_extension;
        l_extension.setAliases(aliases);
        QCOMPARE(l_extension.getAliases() == expected_aliases, expected_result);
    }

    {
        CommandExtension l_extension(name);
        l_extension.setAliases(aliases);
        QCOMPARE(l_extension.getAliases() == expected_aliases, expected_result);
    }

    {
        CommandExtension l_extension;
        l_extension.setCommandName(name);
        l_extension.setAliases(aliases);
        QCOMPARE(l_extension.getAliases() == expected_aliases, expected_result);
    }
}

void tst_CommandExtension::checkAlias_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QStringList>("aliases");
    QTest::addColumn<QString>("target");
    QTest::addColumn<bool>("expected_result");

    QTest::newRow("Target found: name") << "extension" << QStringList{"ext", "extended"} << "extension" << true;
    QTest::newRow("Target found: alias") << "extension" << QStringList{"ext", "extended"} << "ext" << true;
    QTest::newRow("Target not found") << "extension" << QStringList{"ext", "extended"} << "wont_find_me" << false;
}

void tst_CommandExtension::checkAlias()
{
    QFETCH(QString, name);
    QFETCH(QStringList, aliases);
    QFETCH(QString, target);
    QFETCH(bool, expected_result);

    {
        m_extension.setCommandName(name);
        m_extension.setAliases(aliases);
        QCOMPARE(m_extension.checkCommandNameAndAlias(target), expected_result);
    }
}

void tst_CommandExtension::setPermissionsByCaption_data()
{
    QTest::addColumn<QStringList>("permission_captions");
    QTest::addColumn<PermVector>("expected_permissions");
    QTest::addColumn<bool>("message_required");
    QTest::addColumn<bool>("expected_result");

    QTest::addRow("Valid captions") << QStringList{"none", "super"} << PermVector{ACLRole::NONE, ACLRole::SUPER} << false << true;
    QTest::addRow("Invalid captions") << QStringList{"none", "not_none"} << PermVector{ACLRole::NONE, ACLRole::SUPER} << true << false;
    QTest::addRow("Valid and invalid captions") << QStringList{"none", "not_super"} << PermVector{ACLRole::NONE} << true << true;
}

void tst_CommandExtension::setPermissionsByCaption()
{
    QFETCH(QStringList, permission_captions);
    QFETCH(PermVector, expected_permissions);
    QFETCH(bool, message_required);
    QFETCH(bool, expected_result);

    {
        if (message_required) {
            QTest::ignoreMessage(QtWarningMsg, QRegularExpression("\\[Command Extension\\] error: permission \".*?\" does not exist"));
        }
        m_extension.setPermissionsByCaption(permission_captions);
        QCOMPARE(m_extension.getPermissions() == expected_permissions, expected_result);
    }
}

void tst_CommandExtension::checkPermission_data()
{
    QTest::addColumn<PermVector>("permissions");
    QTest::addColumn<PermVector>("default_permissions");
    QTest::addColumn<PermVector>("expected_permissions");
    QTest::addColumn<bool>("expected_result");

    QTest::addRow("Matches permissions") << PermVector{ACLRole::SUPER} << PermVector{} << PermVector{ACLRole::SUPER} << true;
    QTest::addRow("Matches default permissions") << PermVector{} << PermVector{ACLRole::NONE} << PermVector{ACLRole::NONE} << true;
}

void tst_CommandExtension::checkPermission()
{
    QFETCH(PermVector, permissions);
    QFETCH(PermVector, default_permissions);
    QFETCH(PermVector, expected_permissions);
    QFETCH(bool, expected_result);

    {
        m_extension.setPermissions(permissions);
        QCOMPARE(m_extension.getPermissions(default_permissions) == expected_permissions, expected_result);
    }
}

}
}

QTEST_APPLESS_MAIN(tests::unittests::tst_CommandExtension)

#include "tst_unittest_command_extension.moc"
