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

    void unregisterAll(const QString &f_owner_id);

    std::optional<CommandSpec> spec(const QString &f_command_name) const;
    CommandHandler handler(const QString &f_command_name) const;
    QStringList commandNames() const;
    bool contains(const QString &f_command_name) const;

    // True when the permission list is empty or any entry resolves - the
    // exact any-of gate the dispatcher applies to the matched form.
    static bool passesAnyOf(const QStringList &f_permissions, const std::function<bool(const QString &)> &f_can_perform);

    // True when any form of the command is open to a caller whose
    // permissions f_can_perform answers for; false for unknown commands.
    // /commands and /help list through this so they mirror the dispatcher.
    bool canUse(const QString &f_command, const std::function<bool(const QString &)> &f_can_perform) const;

    void applyExtensions(const QString &f_path);

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
    QThread *m_owner_thread;
};

} // namespace akashi
