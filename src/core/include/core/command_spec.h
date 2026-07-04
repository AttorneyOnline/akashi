#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

namespace akashi {

class CommandContext;

using CommandHandler = std::function<void(CommandContext &)>;

// One gated form of a command. Variants let one command carry different
// permission requirements per argument shape, resolved at the dispatch gate
// before the handler runs - no permission check hides inside a handler body,
// so extension overrides and the command list see every gate.
struct CommandVariant
{
    QString id; // names the form for extension overrides ("command.id")
    int min_args = 0;
    int max_args = -1; // -1 accepts any count at or above min_args
    QStringList permissions; // any-of, checked at the dispatch gate
    QString usage;
    QString description;
    CommandHandler handler;
    QString owner_id; // set by the registry; plugin unload sweeps by it

    bool matches(int f_argc) const
    {
        return f_argc >= min_args && (max_args < 0 || f_argc <= max_args);
    }
};

struct CommandSpec
{
    QString name;
    QStringList aliases;
    QStringList permissions;
    int min_args = 0;
    QString usage;
    QString description;
    int sensitive_args_from = -1;

    // When non-empty, dispatch picks the first variant whose argument count
    // matches and gates on ITS permissions; the spec-level permissions,
    // min_args and registered handler are not consulted.
    QList<CommandVariant> variants;

    // The first variant taking this many arguments, or nullptr when no form
    // does. Declaration order decides ties, so narrower forms come first.
    const CommandVariant *match(int f_argc) const
    {
        for (const CommandVariant &l_variant : variants) {
            if (l_variant.matches(f_argc)) {
                return &l_variant;
            }
        }
        return nullptr;
    }
};

} // namespace akashi
