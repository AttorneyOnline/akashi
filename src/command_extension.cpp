#include "command_extension.h"

#include <QDebug>
#include <QSettings>

#include "akashidefs.h"

CommandExtension::CommandExtension() {}

CommandExtension::CommandExtension(QString f_command_name)
{
    setCommandName(f_command_name);
}

CommandExtension::~CommandExtension() {}

QString CommandExtension::getCommandName() const
{
    return m_command_name;
}

void CommandExtension::setCommandName(QString f_command_name)
{
    m_command_name = f_command_name;
    updateMergedAliases();
}

bool CommandExtension::checkCommandNameAndAlias(QString f_alias) const
{
    return m_merged_aliases.contains(f_alias, Qt::CaseInsensitive);
}

QStringList CommandExtension::getAliases() const
{
    return m_aliases;
}

void CommandExtension::setAliases(QStringList f_aliases)
{
    m_aliases = f_aliases;
    for (QString &i_alias : m_aliases) {
        i_alias = i_alias.toLower();
    }
    updateMergedAliases();
}

QVector<ACLRole::Permission> CommandExtension::getPermissions(QVector<ACLRole::Permission> f_defaultPermissions) const
{
    return m_permissions.isEmpty() ? f_defaultPermissions : m_permissions;
}

QVector<ACLRole::Permission> CommandExtension::getPermissions() const
{
    return getPermissions(QVector<ACLRole::Permission>{});
}

void CommandExtension::setPermissions(QVector<ACLRole::Permission> f_permissions)
{
    m_permissions = f_permissions;
}

void CommandExtension::setPermissionsByCaption(QStringList f_captions)
{
    QVector<ACLRole::Permission> l_permissions;
    const QStringList l_permission_captions = ACLRole::PERMISSION_CAPTIONS.values();
    for (const QString &i_caption : qAsConst(f_captions)) {
        const QString l_lower_caption = i_caption.toLower();
        if (!l_permission_captions.contains(l_lower_caption)) {
            qWarning() << "[Command Extension]"
                       << "error: permission" << i_caption << "does not exist";
            continue;
        }
        l_permissions.append(ACLRole::PERMISSION_CAPTIONS.key(l_lower_caption));
    }
    setPermissions(l_permissions);
}

void CommandExtension::updateMergedAliases()
{
    m_merged_aliases = QStringList{m_command_name} + m_aliases;
}

CommandExtensionCollection::CommandExtensionCollection(QObject *parent) :
    QObject(parent)
{}

CommandExtensionCollection::~CommandExtensionCollection() {}

void CommandExtensionCollection::setCommandNameWhitelist(QStringList f_command_names)
{
    m_command_name_whitelist = f_command_names;
    for (QString &i_alias : m_command_name_whitelist) {
        i_alias = i_alias.toLower();
    }
}

QList<CommandExtension> CommandExtensionCollection::getExtensions() const
{
    return m_extensions.values();
}

bool CommandExtensionCollection::containsExtension(QString f_command_name) const
{
    return m_extensions.contains(f_command_name);
}

CommandExtension CommandExtensionCollection::getExtension(QString f_command_name) const
{
    return m_extensions.value(f_command_name);
}

bool CommandExtensionCollection::loadFile(QString f_filename)
{
    QSettings l_settings(f_filename, QSettings::IniFormat);
    if (l_settings.status() != QSettings::NoError) {
        qWarning() << "[Command Extension Collection]"
                   << "error: failed to load file" << f_filename << "; aborting";
        return false;
    }

    m_extensions.clear();
    QStringList l_alias_records;
    QStringList l_command_records;
    const QStringList l_group_list = l_settings.childGroups();
    for (const QString &i_group : l_group_list) {
        const QString l_command_name = i_group.toLower();
        if (!m_command_name_whitelist.isEmpty() && !m_command_name_whitelist.contains(l_command_name)) {
            qWarning() << "[Command Extension Collection]"
                       << "error: command" << l_command_name << "cannot be extended; does not exist";
            continue;
        }

        if (l_command_records.contains(l_command_name)) {
            qWarning() << "[Command Extension Collection]"
                       << "warning: command extension" << l_command_name << "already exist";
            continue;
        }
        l_command_records.append(l_command_name);

        l_settings.beginGroup(i_group);

        QStringList l_aliases = l_settings.value("aliases").toString().split(" ", akashi::SkipEmptyParts);
        for (QString &i_alias : l_aliases) {
            i_alias = i_alias.toLower();
        }

        for (const QString &i_recorded_alias : l_alias_records) {
            if (l_aliases.contains(i_recorded_alias)) {
                qWarning() << "[Command Extension Collection]"
                           << "warning: command alias" << i_recorded_alias << "was already defined";
                l_aliases.removeAll(i_recorded_alias);
            }
        }
        l_alias_records.append(l_aliases);

        CommandExtension l_extension(l_command_name);
        l_extension.setAliases(l_aliases);
        l_extension.setPermissionsByCaption(l_settings.value("permissions").toString().split(" ", akashi::SkipEmptyParts));
        m_extensions.insert(l_command_name, std::move(l_extension));

        l_settings.endGroup();
    }

    return true;
}
