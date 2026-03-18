#include "akashi/config_store.h"

#include "core/json_settings.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>

namespace akashi {

ConfigStore::ConfigStore(const QString &f_root, QObject *parent) :
    QObject(parent),
    m_root(f_root)
{}

QString ConfigStore::rootPath() const
{
    return m_root;
}

QString ConfigStore::filePath(const QString &f_file_name) const
{
    return m_root + "/" + f_file_name;
}

QSettings *ConfigStore::settings(const QString &f_name)
{
    QSettings *l_settings = m_open_settings.value(f_name);
    if (l_settings) {
        return l_settings;
    }

    migrateIniFile(f_name);
    l_settings = new QSettings(filePath(f_name + ".json"), JsonSettings::format(), this);
    m_open_settings.insert(f_name, l_settings);
    return l_settings;
}

QSettings *ConfigStore::pluginSettings(const QString &f_plugin_id)
{
    QDir(m_root).mkpath("plugins");
    return settings("plugins/" + f_plugin_id);
}

void ConfigStore::reload()
{
    for (QSettings *l_settings : std::as_const(m_open_settings)) {
        l_settings->sync();
    }
    emit configReloaded();
}

QString ConfigStore::resolveRootPath()
{
    const QStringList l_arguments = QCoreApplication::arguments();
    const int l_index = l_arguments.indexOf("--config-root");
    if (l_index != -1 && l_index + 1 < l_arguments.size()) {
        return l_arguments.at(l_index + 1);
    }

    const QString l_env = qEnvironmentVariable("AKASHI_CONFIG_ROOT");
    if (!l_env.isEmpty()) {
        return l_env;
    }

    return QStringLiteral("config");
}

void ConfigStore::migrateIniFile(const QString &f_name)
{
    const QString l_json_path = filePath(f_name + ".json");
    const QString l_ini_path = filePath(f_name + ".ini");
    if (QFileInfo::exists(l_json_path) || !QFileInfo::exists(l_ini_path)) {
        return;
    }

    QSettings l_ini(l_ini_path, QSettings::IniFormat);
    QSettings l_json(l_json_path, JsonSettings::format());
    const QStringList l_keys = l_ini.allKeys();
    for (const QString &l_key : l_keys) {
        l_json.setValue(l_key, l_ini.value(l_key));
    }
    l_json.sync();
    qInfo() << "Converted" << l_ini_path << "to" << l_json_path;
}

} // namespace akashi
