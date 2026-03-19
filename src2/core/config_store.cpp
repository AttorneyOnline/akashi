#include "akashi/config_store.h"

#include "core/json_settings.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>

namespace akashi {

// Converts a raw file value to the declared type. Returns false if it does not fit.
static bool convertValue(const QVariant &f_raw, int f_type_id, QVariant &f_out)
{
    if (f_type_id == QMetaType::Bool) {
        if (f_raw.typeId() == QMetaType::Bool) {
            f_out = f_raw;
            return true;
        }
        const QString l_text = f_raw.toString().toLower();
        if (l_text == "true" || l_text == "1") {
            f_out = true;
            return true;
        }
        if (l_text == "false" || l_text == "0") {
            f_out = false;
            return true;
        }
        return false;
    }
    if (f_type_id == QMetaType::Int) {
        bool l_ok = false;
        f_out = f_raw.toInt(&l_ok);
        return l_ok;
    }
    if (f_type_id == QMetaType::Double) {
        bool l_ok = false;
        f_out = f_raw.toDouble(&l_ok);
        return l_ok;
    }
    if (f_type_id == QMetaType::QStringList) {
        f_out = f_raw.toStringList();
        return true;
    }
    f_out = f_raw.toString();
    return true;
}

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

bool ConfigStore::declare(const QString &f_name, const QList<ConfigEntry> &f_entries)
{
    m_entries.insert(f_name, f_entries);
    settings(f_name);
    return loadDeclaredValues(f_name);
}

bool ConfigStore::declarePlugin(const QString &f_plugin_id, const QList<ConfigEntry> &f_entries)
{
    QDir(m_root).mkpath("plugins");
    return declare("plugins/" + f_plugin_id, f_entries);
}

QVariant ConfigStore::value(const QString &f_name, const QString &f_key) const
{
    const QHash<QString, QVariant> l_values = m_values.value(f_name);
    if (!l_values.contains(f_key)) {
        qWarning() << f_name << "has no declared setting" << f_key;
        return QVariant();
    }
    return l_values.value(f_key);
}

void ConfigStore::setValue(const QString &f_name, const QString &f_key, const QVariant &f_value)
{
    settings(f_name)->setValue(f_key, f_value);
    if (m_values.contains(f_name)) {
        m_values[f_name].insert(f_key, f_value);
    }
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

QString ConfigStore::documentation(const QString &f_name) const
{
    QString l_text = "# " + f_name + ".json\n\n| Setting | Default | Description |\n|---|---|---|\n";
    const QList<ConfigEntry> l_entries = m_entries.value(f_name);
    for (const ConfigEntry &l_entry : l_entries) {
        l_text += "| " + l_entry.key() + " | " + l_entry.defaultValue().toString() + " | " + l_entry.description() + " |\n";
    }
    return l_text;
}

void ConfigStore::reload()
{
    for (QSettings *l_settings : std::as_const(m_open_settings)) {
        l_settings->sync();
    }

    for (auto l_iterator = m_entries.keyBegin(); l_iterator != m_entries.keyEnd(); ++l_iterator) {
        const QString l_name = *l_iterator;
        const QHash<QString, QVariant> l_old_values = m_values.value(l_name);
        // A broken value on reload keeps the old values instead of stopping the server.
        if (!loadDeclaredValues(l_name)) {
            m_values.insert(l_name, l_old_values);
            continue;
        }
        const QHash<QString, QVariant> l_new_values = m_values.value(l_name);
        for (auto l_value_iterator = l_new_values.begin(); l_value_iterator != l_new_values.end(); ++l_value_iterator) {
            if (l_old_values.value(l_value_iterator.key()) != l_value_iterator.value()) {
                emit valueChanged(l_name, l_value_iterator.key(), l_old_values.value(l_value_iterator.key()), l_value_iterator.value());
            }
        }
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
        QVariant l_value = l_ini.value(l_key);
        // Declared settings are written with their real type.
        const ConfigEntry *l_entry = findEntry(f_name, l_key);
        if (l_entry) {
            QVariant l_typed;
            if (convertValue(l_value, l_entry->typeId(), l_typed)) {
                l_value = l_typed;
            }
        }
        l_json.setValue(l_key, l_value);
    }
    l_json.sync();
    qInfo() << "Converted" << l_ini_path << "to" << l_json_path;
}

bool ConfigStore::loadDeclaredValues(const QString &f_name)
{
    QSettings *l_settings = settings(f_name);
    const QList<ConfigEntry> l_entries = m_entries.value(f_name);

    // Unknown keys are reported so typos do not go unnoticed.
    const QStringList l_file_keys = l_settings->allKeys();
    for (const QString &l_key : l_file_keys) {
        if (!findEntry(f_name, l_key)) {
            qWarning() << filePath(f_name + ".json") << "has an unknown setting" << l_key;
        }
    }

    QHash<QString, QVariant> l_values;
    bool l_valid = true;
    for (const ConfigEntry &l_entry : l_entries) {
        const QVariant l_raw = l_settings->value(l_entry.key());
        if (!l_raw.isValid()) {
            l_values.insert(l_entry.key(), l_entry.defaultValue());
            continue;
        }

        QVariant l_value;
        if (!convertValue(l_raw, l_entry.typeId(), l_value)) {
            qCritical() << filePath(f_name + ".json") << l_entry.key() << "must be of type" << QMetaType(l_entry.typeId()).name() << "- got" << l_raw.toString();
            l_valid = false;
            continue;
        }
        if (!l_entry.checkValue(l_value)) {
            qCritical() << filePath(f_name + ".json") << l_entry.key() << "has an invalid value" << l_raw.toString();
            l_valid = false;
            continue;
        }
        l_values.insert(l_entry.key(), l_value);
    }

    m_values.insert(f_name, l_values);
    return l_valid;
}

const ConfigEntry *ConfigStore::findEntry(const QString &f_name, const QString &f_key) const
{
    const auto l_iterator = m_entries.constFind(f_name);
    if (l_iterator == m_entries.constEnd()) {
        return nullptr;
    }
    for (const ConfigEntry &l_entry : l_iterator.value()) {
        if (l_entry.key() == f_key) {
            return &l_entry;
        }
    }
    return nullptr;
}

} // namespace akashi
