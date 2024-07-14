#include "acl_roles_handler.h"

#include <QDebug>
#include <QSettings>

const QString ACLRolesHandler::NONE_ID = "NONE";

const QString ACLRolesHandler::SUPER_ID = "SUPER";

const QHash<QString, ACLRole> ACLRolesHandler::readonly_roles{
    {ACLRolesHandler::NONE_ID, ACLRole(ACLRole::NONE)},
    {ACLRolesHandler::SUPER_ID, ACLRole(ACLRole::SUPER)},
};

const QHash<ACLRole::Permission, QString> ACLRole::PERMISSION_CAPTIONS{
    {
        ACLRole::Permission::NONE,
        "none",
    },
    {
        ACLRole::Permission::KICK,
        "kick",
    },
    {
        ACLRole::Permission::BAN,
        "ban",
    },
    {
        ACLRole::Permission::BGLOCK,
        "lock_background",
    },
    {
        ACLRole::Permission::MODIFY_USERS,
        "modify_users",
    },
    {
        ACLRole::Permission::CM,
        "gamemaster",
    },
    {
        ACLRole::Permission::GLOBAL_TIMER,
        "global_timer",
    },
    {
        ACLRole::Permission::EVI_MOD,
        "modify_evidence",
    },
    {
        ACLRole::Permission::MOTD,
        "motd",
    },
    {
        ACLRole::Permission::ANNOUNCE,
        "announcer",
    },
    {
        ACLRole::Permission::MODCHAT,
        "chat_moderator",
    },
    {
        ACLRole::Permission::MUTE,
        "mute",
    },
    {
        ACLRole::Permission::UNCM,
        "remove_gamemaster",
    },
    {
        ACLRole::Permission::SAVETEST,
        "save_testimony",
    },
    {
        ACLRole::Permission::FORCE_CHARSELECT,
        "force_charselect",
    },
    {
        ACLRole::Permission::BYPASS_LOCKS,
        "bypass_locks",
    },
    {
        ACLRole::Permission::IGNORE_BGLIST,
        "ignore_background_list",
    },
    {
        ACLRole::Permission::SEND_NOTICE,
        "send_notice",
    },
    {
        ACLRole::Permission::JUKEBOX,
        "jukebox",
    },
    {
        ACLRole::Permission::SUPER,
        "super",
    },
};

ACLRole::ACLRole() {}

ACLRole::ACLRole(ACLRole::Permissions f_permissions) :
    m_permissions(f_permissions)
{
}

ACLRole::~ACLRole() {}

ACLRole::Permissions ACLRole::getPermissions() const
{
    return m_permissions;
}

bool ACLRole::checkPermission(Permission f_permission) const
{
    if (f_permission == ACLRole::NONE) {
        return true;
    }
    return m_permissions.testFlag(f_permission);
}

void ACLRole::setPermissions(ACLRole::Permissions f_permissions)
{
    m_permissions = f_permissions;
}

void ACLRole::setPermission(Permission f_permission, bool f_mode)
{
    m_permissions.setFlag(f_permission, f_mode);
}

ACLRolesHandler::ACLRolesHandler(QObject *parent) :
    QObject(parent)
{}

ACLRolesHandler::~ACLRolesHandler() {}

bool ACLRolesHandler::roleExists(QString f_id)
{
    f_id = f_id.toUpper();
    return readonly_roles.contains(f_id) || m_roles.contains(f_id);
}

ACLRole ACLRolesHandler::getRoleById(QString f_id)
{
    f_id = f_id.toUpper();
    return readonly_roles.contains(f_id) ? readonly_roles.value(f_id) : m_roles.value(f_id);
}

bool ACLRolesHandler::insertRole(QString f_id, ACLRole f_role)
{
    f_id = f_id.toUpper();
    if (readonly_roles.contains(f_id)) {
        return false;
    }
    m_roles.insert(f_id, f_role);
    return true;
}

bool ACLRolesHandler::removeRole(QString f_id)
{
    f_id = f_id.toUpper();
    if (readonly_roles.contains(f_id)) {
        return false;
    }
    else if (!m_roles.contains(f_id)) {
        return false;
    }
    m_roles.remove(f_id);
    return true;
}

void ACLRolesHandler::clearRoles()
{
    m_roles.clear();
}

bool ACLRolesHandler::loadFile(QString f_file_name)
{
    QSettings l_settings(f_file_name, QSettings::IniFormat);
    if (!checkPermissionsIni(&l_settings)) {
        return false;
    }

    m_roles.clear();
    QStringList l_role_records;
    const QStringList l_group_list = l_settings.childGroups();
    for (const QString &i_group : l_group_list) {
        const QString l_upper_group = i_group.toUpper();
        if (readonly_roles.contains(l_upper_group)) {
            qWarning() << "[ACL Role Handler]"
                       << "warning: cannot modify role;" << i_group << "is read-only";
            continue;
        }

        l_settings.beginGroup(i_group);
        if (l_role_records.contains(l_upper_group)) {
            qWarning() << "[ACL Role Handler]"
                       << "warning: role" << l_upper_group << "already exist";
            continue;
        }
        l_role_records.append(l_upper_group);

        ACLRole l_role;
        const QList<ACLRole::Permission> l_permissions = ACLRole::PERMISSION_CAPTIONS.keys();
        for (const ACLRole::Permission &i_permission : l_permissions) {
            const QVariant l_value = l_settings.value(ACLRole::PERMISSION_CAPTIONS.value(i_permission));
            if (l_value.isValid()) {
                l_role.setPermission(i_permission, l_value.toBool());
            }
        }
        m_roles.insert(l_upper_group, std::move(l_role));
        l_settings.endGroup();
    }

    return true;
}

bool ACLRolesHandler::saveFile(QString f_file_name)
{
    QSettings l_settings(f_file_name, QSettings::IniFormat);
    if (!checkPermissionsIni(&l_settings)) {
        return false;
    }

    l_settings.clear();
    const QStringList l_role_id_list = m_roles.keys();
    for (const QString &l_role_id : l_role_id_list) {
        const QString l_upper_role_id = l_role_id.toUpper();
        if (readonly_roles.contains(l_upper_role_id)) {
            continue;
        }

        const ACLRole i_role = m_roles.value(l_upper_role_id);
        l_settings.beginGroup(l_upper_role_id);
        if (i_role.checkPermission(ACLRole::SUPER)) {
            l_settings.setValue(ACLRole::PERMISSION_CAPTIONS.value(ACLRole::SUPER), true);
        }
        else {
            const QList<ACLRole::Permission> l_permissions = ACLRole::PERMISSION_CAPTIONS.keys();
            for (const ACLRole::Permission i_permission : l_permissions) {
                if (!i_role.checkPermission(i_permission)) {
                    continue;
                }
                l_settings.setValue(ACLRole::PERMISSION_CAPTIONS.value(i_permission), true);
            }
        }
        l_settings.endGroup();
    }
    l_settings.sync();
    if (l_settings.status() != QSettings::NoError) {
        qWarning() << "[ACL Role Handler]"
                   << "error: failed to write file; aborting (" << f_file_name << ")";
        return false;
    }

    return true;
}

bool ACLRolesHandler::checkPermissionsIni(QSettings *f_settings)
{
    if (f_settings->status() != QSettings::NoError) {
        switch (f_settings->status()) {
        case QSettings::AccessError:
            qWarning() << "[ACL Role Handler]"
                       << "error: failed to open file; aborting (" << f_settings->fileName() << ")";
            break;

        case QSettings::FormatError:
            qWarning() << "[ACL Role Handler]"
                       << "error: file is malformed; aborting (" << f_settings->fileName() << ")";
            break;

        default:
            qWarning() << "[ACL Role Handler]"
                       << "error: unknown error; aborting; aborting (" << f_settings->fileName() << ")";
            break;
        }

        return false;
    }
    return true;
}
