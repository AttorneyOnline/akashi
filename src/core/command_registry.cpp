#include "core/command_registry.h"

#include "akashi/thread_assert.h"

#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace akashi {

CommandRegistry::CommandRegistry() :
    m_owner_thread(QThread::currentThread())
{}

QString CommandRegistry::serviceId() const
{
    return QStringLiteral("akashi.commands");
}

ServiceVersion CommandRegistry::serviceVersion() const
{
    return {1, 0, 0};
}

bool CommandRegistry::registerCommand(const CommandSpec &f_spec, CommandHandler f_handler,
                                      const QString &f_owner_id)
{
    AKASHI_ASSERT_OWNER_THREAD();
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

    for (const CommandVariant &l_variant : f_spec.variants) {
        if (l_variant.id.isEmpty() || !l_variant.handler) {
            return false;
        }
    }

    Entry l_entry{f_spec, std::move(f_handler), f_owner_id};
    // The command's own variants belong to its owner.
    for (CommandVariant &l_variant : l_entry.spec.variants) {
        l_variant.owner_id = f_owner_id;
    }
    m_entries.insert(l_key, std::move(l_entry));
    for (const QString &l_alias : f_spec.aliases) {
        m_aliases.insert(l_alias.toLower(), l_key);
    }
    return true;
}

bool CommandRegistry::registerCommand(const CommandSpec &f_spec, const QString &f_owner_id)
{
    if (f_spec.variants.isEmpty()) {
        return false;
    }
    return registerCommand(f_spec, CommandHandler(), f_owner_id);
}

bool CommandRegistry::registerVariant(const QString &f_command_name, const CommandVariant &f_variant,
                                      const QString &f_owner_id)
{
    AKASHI_ASSERT_OWNER_THREAD();
    auto l_entry = m_entries.find(resolve(f_command_name));
    if (l_entry == m_entries.end() || l_entry->spec.variants.isEmpty()) {
        return false;
    }
    if (f_variant.id.isEmpty() || !f_variant.handler) {
        return false;
    }
    for (const CommandVariant &l_existing : l_entry->spec.variants) {
        if (l_existing.id == f_variant.id) {
            return false;
        }
        // Overlapping windows would shadow the appended form partially.
        const bool l_below = f_variant.max_args >= 0 && f_variant.max_args < l_existing.min_args;
        const bool l_above = l_existing.max_args >= 0 && l_existing.max_args < f_variant.min_args;
        if (!l_below && !l_above) {
            return false;
        }
    }

    CommandVariant l_variant = f_variant;
    l_variant.owner_id = f_owner_id;
    l_entry->spec.variants.append(std::move(l_variant));
    return true;
}

void CommandRegistry::unregisterAll(const QString &f_owner_id)
{
    AKASHI_ASSERT_OWNER_THREAD();
    auto it = m_entries.begin();
    while (it != m_entries.end()) {
        if (it->owner_id == f_owner_id) {
            for (const QString &l_alias : it->spec.aliases) {
                m_aliases.remove(l_alias.toLower());
            }
            it = m_entries.erase(it);
        }
        else {
            // A surviving command may still carry the owner's added variants.
            it->spec.variants.removeIf([&f_owner_id](const CommandVariant &v) {
                return v.owner_id == f_owner_id;
            });
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
    AKASHI_ASSERT_OWNER_THREAD();
    const QString l_key = resolve(f_command_name);
    if (auto it = m_entries.constFind(l_key); it != m_entries.constEnd()) {
        return it->spec;
    }
    return std::nullopt;
}

CommandHandler CommandRegistry::handler(const QString &f_command_name) const
{
    AKASHI_ASSERT_OWNER_THREAD();
    const QString l_key = resolve(f_command_name);
    if (auto it = m_entries.constFind(l_key); it != m_entries.constEnd()) {
        return it->handler;
    }
    return {};
}

QStringList CommandRegistry::commandNames() const
{
    AKASHI_ASSERT_OWNER_THREAD();
    return m_entries.keys();
}

bool CommandRegistry::contains(const QString &f_command_name) const
{
    AKASHI_ASSERT_OWNER_THREAD();
    return !resolve(f_command_name).isEmpty();
}

void CommandRegistry::applyExtensions(const QString &f_path)
{
    AKASHI_ASSERT_OWNER_THREAD();
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
        QString l_name = it.key().toLower();

        // "command.variant" addresses one gated form of a variant command.
        QString l_variant_id;
        if (const int l_dot = l_name.indexOf(QChar('.')); l_dot >= 0) {
            l_variant_id = l_name.mid(l_dot + 1);
            l_name = l_name.left(l_dot);
        }

        auto l_entry = m_entries.find(l_name);
        if (l_entry == m_entries.end()) {
            qWarning() << "command_extensions: unknown command" << l_name;
            continue;
        }

        const QJsonObject l_ext = it.value().toObject();

        if (!l_variant_id.isEmpty()) {
            bool l_found = false;
            for (CommandVariant &l_variant : l_entry->spec.variants) {
                if (l_variant.id == l_variant_id) {
                    if (l_ext.contains(QStringLiteral("permissions"))) {
                        l_variant.permissions = l_ext.value(QStringLiteral("permissions")).toString().split(QChar(' '), Qt::SkipEmptyParts);
                    }
                    l_found = true;
                    break;
                }
            }
            if (!l_found) {
                qWarning() << "command_extensions:" << l_name << "has no variant" << l_variant_id;
            }
            continue;
        }

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
            if (!l_entry->spec.variants.isEmpty()) {
                qWarning() << "command_extensions:" << l_name << "has gated forms; address one as"
                           << l_name + QStringLiteral(".<variant>") << "instead";
            }
            else {
                l_entry->spec.permissions = l_ext.value(QStringLiteral("permissions")).toString().split(QChar(' '), Qt::SkipEmptyParts);
            }
        }
    }
}

} // namespace akashi
