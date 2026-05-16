#ifndef CORE_COMMAND_REGISTRY_H
#define CORE_COMMAND_REGISTRY_H

#include "akashi_core_export.h"
#include "core/command_spec.h"

#include <QHash>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

namespace akashi {

class CommandContext;

using CommandHandler = std::function<void(CommandContext &)>;

class AKASHI_CORE_EXPORT CommandRegistry
{
  public:
    bool registerCommand(const CommandSpec &f_spec, CommandHandler f_handler,
                         const QString &f_owner_id = {});
    void unregisterAll(const QString &f_owner_id);

    std::optional<CommandSpec> spec(const QString &f_command_name) const;
    CommandHandler handler(const QString &f_command_name) const;
    QStringList commandNames() const;
    bool contains(const QString &f_command_name) const;

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
};

} // namespace akashi

#endif // CORE_COMMAND_REGISTRY_H
