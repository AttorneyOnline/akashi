#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

namespace akashi {

class CommandContext;

using CommandHandler = std::function<void(CommandContext &)>;

// A shadow's way forward: invoke it with the (possibly rewritten)
// argument list, and the gate plus variant selection re-run on those
// arguments before anything deeper executes - a rewrite can never
// smuggle arguments past the gate. Not calling it swallows the command.
using CommandNext = std::function<void(const QStringList &)>;

// A shadow over an existing command: it runs in the command's place and
// decides whether, and with what arguments, the wrapped binding
// proceeds. This is the packet-interceptor pattern on the command chain.
using CommandShadowFn = std::function<void(CommandContext &, const CommandNext &)>;

struct CommandShadow
{
    int priority = 0; // higher runs earlier (outermost)
    CommandShadowFn shadow;
    QString owner_id;
};

// One gated form of a command. Variants let one command carry different
// permission requirements per argument shape, resolved at the dispatch gate
// before the handler runs - no permission check hides inside a handler body,
// so extension overrides and the command list see every gate.
struct CommandVariant
{
    QString id; // names the form for extension overrides ("command.id")
    int min_args = 0;
    int max_args = -1;       // -1 accepts any count at or above min_args
    QStringList permissions; // any-of, checked at the dispatch gate
    QString usage;
    QString description;
    CommandHandler handler;
    QString owner_id; // set by the registry; plugin unload sweeps by it

    // Any-of over all-of groups: {{"area.cm","kick"}} needs both,
    // {{"area.cm"},{"music.jukebox"}} needs either. When non-empty this
    // is the form's whole gate and the flat permissions list is ignored.
    QList<QStringList> requirement_groups;

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

    // The spec-level AND gate, same shape as the variant field.
    QList<QStringList> requirement_groups;

    // A value-dependent escalation the body triggers but the spec names,
    // so /help shows it and an extension can override it: timer 0 needs
    // this, the SUPER role as a /setperms argument needs this.
    QString escalates_to;
    QString escalates_when; // the human words for /help

    // A target holding this permission is immune to the command - "you
    // cannot kick another CM" - declared here instead of hidden inline,
    // enforced through CommandContext::targetIsImmune.
    QString target_immune_if;

    // The first variant taking this many arguments, or nullptr when no form
    // does. Declaration order decides ties, so narrower forms come first.
    // The result points into this spec's own storage, so bind the spec to a
    // named variable first - never call match() on a CommandRegistry lookup
    // returned by value (e.g. registry.spec(name)->match(argc)), or the
    // pointer dangles once the temporary spec dies.
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
