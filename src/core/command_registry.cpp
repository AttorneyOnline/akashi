#include "core/command_registry.h"

#include "akashi/logging_categories.h"
#include "akashi/thread_assert.h"

#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <utility>

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

bool CommandRegistry::shadowCommand(const QString &f_command_name, int f_priority, CommandShadowFn f_shadow,
                                    const QString &f_owner_id)
{
    AKASHI_ASSERT_OWNER_THREAD();
    const QString l_key = resolve(f_command_name);
    if (l_key.isEmpty() || !f_shadow) {
        return false;
    }
    QList<CommandShadow> &l_stack = m_shadows[l_key];
    CommandShadow l_entry{f_priority, std::move(f_shadow), f_owner_id};
    // Insert after equal priorities, so ties keep registration order and
    // a later shadow can never preempt an earlier one at the same rank.
    auto l_pos = std::upper_bound(l_stack.cbegin(), l_stack.cend(), l_entry,
                                  [](const CommandShadow &a, const CommandShadow &b) {
                                      return a.priority > b.priority;
                                  });
    l_stack.insert(l_pos, std::move(l_entry));
    return true;
}

QList<CommandShadow> CommandRegistry::shadowsOf(const QString &f_command_name) const
{
    AKASHI_ASSERT_OWNER_THREAD();
    return m_shadows.value(resolve(f_command_name));
}

void CommandRegistry::unregisterAll(const QString &f_owner_id)
{
    AKASHI_ASSERT_OWNER_THREAD();
    m_entries.removeIf([this, &f_owner_id](std::pair<const QString &, Entry &> f_item) {
        Entry &l_entry = f_item.second;
        if (l_entry.owner_id == f_owner_id) {
            for (const QString &l_alias : l_entry.spec.aliases) {
                m_aliases.remove(l_alias.toLower());
            }
            // A dead command's shadow stack must not survive onto a later
            // command re-registered under the same name.
            m_shadows.remove(f_item.first);
            return true;
        }
        // A surviving command may still carry the owner's added variants.
        l_entry.spec.variants.removeIf([&f_owner_id](const CommandVariant &v) {
            return v.owner_id == f_owner_id;
        });
        return false;
    });
    for (auto it = m_shadows.begin(); it != m_shadows.end();) {
        it->removeIf([&f_owner_id](const CommandShadow &s) {
            return s.owner_id == f_owner_id;
        });
        if (it->isEmpty()) {
            it = m_shadows.erase(it);
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

std::optional<CommandRegistry::Resolved> CommandRegistry::lookup(const QString &f_command_name) const
{
    AKASHI_ASSERT_OWNER_THREAD();
    const QString l_key = resolve(f_command_name);
    if (auto it = m_entries.constFind(l_key); it != m_entries.constEnd()) {
        return Resolved{it->spec, it->handler};
    }
    return std::nullopt;
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

bool CommandRegistry::passesAnyOf(const QStringList &f_permissions, const std::function<bool(const QString &)> &f_can_perform)
{
    if (f_permissions.isEmpty()) {
        return true;
    }
    for (const QString &l_permission : f_permissions) {
        if (f_can_perform(l_permission)) {
            return true;
        }
    }
    return false;
}

bool CommandRegistry::passesRequirements(const QStringList &f_permissions, const QList<QStringList> &f_groups,
                                         const std::function<bool(const QString &)> &f_can_perform)
{
    if (f_groups.isEmpty()) {
        return passesAnyOf(f_permissions, f_can_perform);
    }
    for (const QStringList &l_group : f_groups) {
        bool l_all = true;
        for (const QString &l_permission : l_group) {
            if (!f_can_perform(l_permission)) {
                l_all = false;
                break;
            }
        }
        if (l_all) {
            return true;
        }
    }
    return false;
}

bool CommandRegistry::canUse(const QString &f_command, const std::function<bool(const QString &)> &f_can_perform) const
{
    AKASHI_ASSERT_OWNER_THREAD();
    const auto l_spec = spec(f_command);
    if (!l_spec) {
        return false;
    }
    if (l_spec->variants.isEmpty()) {
        return passesRequirements(l_spec->permissions, l_spec->requirement_groups, f_can_perform);
    }
    for (const CommandVariant &l_variant : l_spec->variants) {
        if (passesRequirements(l_variant.permissions, l_variant.requirement_groups, f_can_perform)) {
            return true;
        }
    }
    return false;
}

// Reads one extension permission string into the flat list and the AND
// groups: "gamemaster+kick ban" is (gamemaster AND kick) OR ban. Answers
// false - the override must be skipped whole - when the validator knows
// a name is a typo; a half-applied gate could be softer than intended.
static bool parseExtensionGate(const QString &f_text, const std::function<bool(const QString &)> &f_known,
                               QStringList &f_permissions, QList<QStringList> &f_groups)
{
    f_permissions.clear();
    f_groups.clear();
    const QStringList l_terms = f_text.split(QChar(' '), Qt::SkipEmptyParts);
    for (const QString &l_term : l_terms) {
        const QStringList l_members = l_term.split(QChar('+'), Qt::SkipEmptyParts);
        for (const QString &l_member : l_members) {
            if (f_known && !f_known(l_member)) {
                qCWarning(akashiCommands) << "command_extensions: unknown permission" << l_member << "- the override was skipped";
                return false;
            }
        }
        if (l_members.size() > 1) {
            f_groups.append(l_members);
        }
        else if (!l_members.isEmpty()) {
            f_permissions.append(l_members.first());
        }
    }
    // Mixed input compiles to groups only, so one mechanism gates the form.
    if (!f_groups.isEmpty()) {
        for (const QString &l_single : std::as_const(f_permissions)) {
            f_groups.append(QStringList{l_single});
        }
        f_permissions.clear();
    }
    return true;
}

void CommandRegistry::applyExtensions(const QString &f_path, const std::function<bool(const QString &)> &f_known_permission)
{
    AKASHI_ASSERT_OWNER_THREAD();
    QFile l_file(f_path);
    if (!l_file.open(QIODevice::ReadOnly)) {
        return;
    }
    QJsonDocument l_doc = QJsonDocument::fromJson(l_file.readAll());
    if (!l_doc.isObject()) {
        qCWarning(akashiCommands) << f_path << "is not a valid command extensions file";
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
            qCWarning(akashiCommands) << "command_extensions: unknown command" << l_name;
            continue;
        }

        const QJsonObject l_ext = it.value().toObject();

        if (!l_variant_id.isEmpty()) {
            bool l_found = false;
            for (CommandVariant &l_variant : l_entry->spec.variants) {
                if (l_variant.id == l_variant_id) {
                    if (l_ext.contains(QStringLiteral("permissions"))) {
                        QStringList l_permissions;
                        QList<QStringList> l_groups;
                        if (parseExtensionGate(l_ext.value(QStringLiteral("permissions")).toString(), f_known_permission, l_permissions, l_groups)) {
                            l_variant.permissions = l_permissions;
                            l_variant.requirement_groups = l_groups;
                        }
                    }
                    l_found = true;
                    break;
                }
            }
            if (!l_found) {
                qCWarning(akashiCommands) << "command_extensions:" << l_name << "has no variant" << l_variant_id;
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
                    qCWarning(akashiCommands) << "command_extensions:" << l_name << "alias" << l_alias << "collides with an existing command or alias";
                    continue;
                }
                l_entry->spec.aliases.append(l_alias_key);
                m_aliases.insert(l_alias_key, l_name);
            }
        }

        if (l_ext.contains(QStringLiteral("permissions"))) {
            if (!l_entry->spec.variants.isEmpty()) {
                qCWarning(akashiCommands) << "command_extensions:" << l_name << "has gated forms; address one as"
                                          << l_name + QStringLiteral(".<variant>") << "instead";
            }
            else {
                QStringList l_permissions;
                QList<QStringList> l_groups;
                if (parseExtensionGate(l_ext.value(QStringLiteral("permissions")).toString(), f_known_permission, l_permissions, l_groups)) {
                    l_entry->spec.permissions = l_permissions;
                    l_entry->spec.requirement_groups = l_groups;
                }
            }
        }
    }
}

} // namespace akashi
