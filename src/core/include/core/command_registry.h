#pragma once

#include "akashi/service.h"
#include "akashi_core_export.h"
#include "core/command_spec.h"

#include <QHash>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

class QThread;

namespace akashi {

class CommandContext;

class AKASHI_CORE_EXPORT CommandRegistry : public IService
{
  public:
    CommandRegistry();

    QString serviceId() const override;
    ServiceVersion serviceVersion() const override;

    bool registerCommand(const CommandSpec &f_spec, CommandHandler f_handler,
                         const QString &f_owner_id = {});

    // Registers a command whose forms live in spec.variants; every variant
    // must carry an id and a handler.
    bool registerCommand(const CommandSpec &f_spec, const QString &f_owner_id);

    // Adds another gated form to an existing variant-based command. Refuses
    // unknown commands, duplicate ids, argument windows a declared form
    // already covers, and commands without variants (their single handler
    // would become unreachable).
    bool registerVariant(const QString &f_command_name, const CommandVariant &f_variant,
                         const QString &f_owner_id = {});

    // Stacks a shadow over an existing command: higher priority runs
    // earlier, ties keep registration order. The dispatcher re-gates on
    // every argument list a shadow hands forward, so a shadow can
    // neither soften nor sharpen the verb it wraps. Shadows pop with
    // their owner, restoring the original binding; an unknown command
    // is refused.
    bool shadowCommand(const QString &f_command_name, int f_priority, CommandShadowFn f_shadow,
                       const QString &f_owner_id = {});

    // The command's shadow stack, outermost first.
    QList<CommandShadow> shadowsOf(const QString &f_command_name) const;

    void unregisterAll(const QString &f_owner_id);

    std::optional<CommandSpec> spec(const QString &f_command_name) const;
    CommandHandler handler(const QString &f_command_name) const;
    QStringList commandNames() const;
    bool contains(const QString &f_command_name) const;

    struct Resolved
    {
        CommandSpec spec;
        CommandHandler handler;
    };

    // The dispatcher's single lookup: the command's spec and handler
    // together, alias-resolved once.
    std::optional<Resolved> lookup(const QString &f_command_name) const;

    // True when the permission list is empty or any entry resolves - the
    // exact any-of gate the dispatcher applies to the matched form.
    static bool passesAnyOf(const QStringList &f_permissions, const std::function<bool(const QString &)> &f_can_perform);

    // The whole gate of one form: with requirement groups declared, any
    // group whose every member resolves passes; otherwise the flat any-of
    // list decides. The dispatcher and canUse share this.
    static bool passesRequirements(const QStringList &f_permissions, const QList<QStringList> &f_groups,
                                   const std::function<bool(const QString &)> &f_can_perform);

    // True when any form of the command is open to a caller whose
    // permissions f_can_perform answers for; false for unknown commands.
    // /commands and /help list through this so they mirror the dispatcher.
    bool canUse(const QString &f_command, const std::function<bool(const QString &)> &f_can_perform) const;

    // Applies command_extensions.json. The validator answers whether a
    // permission name exists; an override naming an unknown permission is
    // skipped loudly instead of loading a broken gate. A "+" joins the
    // members of an all-of group: "gamemaster+kick ban" reads as
    // (gamemaster AND kick) OR ban.
    void applyExtensions(const QString &f_path, const std::function<bool(const QString &)> &f_known_permission = {});

  private:
    struct Entry
    {
        CommandSpec spec;
        CommandHandler handler;
        QString owner_id;
    };

    QString resolve(const QString &f_name) const;

    QHash<QString, Entry> m_entries;
    QHash<QString, QString> m_aliases;
    QHash<QString, QList<CommandShadow>> m_shadows;
    QThread *m_owner_thread;
};

} // namespace akashi
