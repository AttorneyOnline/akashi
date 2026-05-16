#include "core/command_registry.h"

#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace akashi {

bool CommandRegistry::registerCommand(const CommandSpec &f_spec, CommandHandler f_handler,
                                      const QString &f_owner_id)
{
    const QString l_key = f_spec.name.toLower();
    if (m_entries.contains(l_key) || m_aliases.contains(l_key)) {
        return false;
    }

    for (const QString &l_alias : f_spec.aliases) {
        const QString l_alias_key = l_alias.toLower();
        if (m_entries.contains(l_alias_key) || m_aliases.contains(l_alias_key)) {
            return false;
        }
    }

    m_entries.insert(l_key, {f_spec, std::move(f_handler), f_owner_id});
    for (const QString &l_alias : f_spec.aliases) {
        m_aliases.insert(l_alias.toLower(), l_key);
    }
    return true;
}

void CommandRegistry::unregisterAll(const QString &f_owner_id)
{
    auto it = m_entries.begin();
    while (it != m_entries.end()) {
        if (it->owner_id == f_owner_id) {
            for (const QString &l_alias : it->spec.aliases) {
                m_aliases.remove(l_alias.toLower());
            }
            it = m_entries.erase(it);
        }
        else {
            ++it;
        }
    }
}

QString CommandRegistry::resolve(const QString &f_name) const
{
    const QString l_key = f_name.toLower();
    if (m_entries.contains(l_key)) {
        return l_key;
    }
    return m_aliases.value(l_key);
}

std::optional<CommandSpec> CommandRegistry::spec(const QString &f_command_name) const
{
    const QString l_key = resolve(f_command_name);
    if (auto it = m_entries.constFind(l_key); it != m_entries.constEnd()) {
        return it->spec;
    }
    return std::nullopt;
}

CommandHandler CommandRegistry::handler(const QString &f_command_name) const
{
    const QString l_key = resolve(f_command_name);
    if (auto it = m_entries.constFind(l_key); it != m_entries.constEnd()) {
        return it->handler;
    }
    return {};
}

QStringList CommandRegistry::commandNames() const
{
    return m_entries.keys();
}

bool CommandRegistry::contains(const QString &f_command_name) const
{
    return !resolve(f_command_name).isEmpty();
}

void CommandRegistry::applyExtensions(const QString &f_path)
{
    QFile l_file(f_path);
    if (!l_file.open(QIODevice::ReadOnly)) {
        return;
    }
    QJsonDocument l_doc = QJsonDocument::fromJson(l_file.readAll());
    if (!l_doc.isObject()) {
        qWarning() << f_path << "is not a valid command extensions file";
        return;
    }

    const QJsonObject l_root = l_doc.object();
    for (auto it = l_root.begin(); it != l_root.end(); ++it) {
        const QString l_name = it.key().toLower();
        auto l_entry = m_entries.find(l_name);
        if (l_entry == m_entries.end()) {
            qWarning() << "command_extensions: unknown command" << l_name;
            continue;
        }

        const QJsonObject l_ext = it.value().toObject();

        if (l_ext.contains(QStringLiteral("aliases"))) {
            const QStringList l_new_aliases = l_ext.value(QStringLiteral("aliases")).toString().split(QChar(' '), Qt::SkipEmptyParts);
            for (const QString &l_alias : l_new_aliases) {
                const QString l_alias_key = l_alias.toLower();
                if (resolve(l_alias_key) == l_name) {
                    continue;
                }
                if (m_entries.contains(l_alias_key) || m_aliases.contains(l_alias_key)) {
                    qWarning() << "command_extensions:" << l_name << "alias" << l_alias << "collides with an existing command or alias";
                    continue;
                }
                l_entry->spec.aliases.append(l_alias_key);
                m_aliases.insert(l_alias_key, l_name);
            }
        }

        if (l_ext.contains(QStringLiteral("permissions"))) {
            l_entry->spec.permissions = l_ext.value(QStringLiteral("permissions")).toString().split(QChar(' '), Qt::SkipEmptyParts);
        }
    }
}

} // namespace akashi
