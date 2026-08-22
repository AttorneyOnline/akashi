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

// A command form's whole gate: any-of over all-of. One permission is a
// group of one, so the ordinary case still reads as a plain name -
// {"area.cm","kick"} in one group needs both, two groups of one needs
// either. An empty gate is an open command.
struct Gate
{
    Gate() = default;
    Gate(const QString &f_permission) :
        groups{{f_permission}}
    {}
    Gate(std::initializer_list<QString> f_any_of)
    {
        for (const QString &l_permission : f_any_of) {
            groups.append({l_permission});
        }
    }
    Gate(const QStringList &f_any_of)
    {
        for (const QString &l_permission : f_any_of) {
            groups.append({l_permission});
        }
    }

    // Every named permission must resolve for this to pass.
    static Gate allOf(const QStringList &f_permissions)
    {
        Gate l_gate;
        if (!f_permissions.isEmpty()) {
            l_gate.groups.append(f_permissions);
        }
        return l_gate;
    }

    QList<QStringList> groups;

    bool isEmpty() const { return groups.isEmpty(); }

    // The gate itself, asked the one way. The dispatcher, /commands and
    // /help all call this, so a listing can never disagree with what
    // dispatch actually does.
    bool passes(const std::function<bool(const QString &)> &f_can_perform) const
    {
        if (groups.isEmpty()) {
            return true;
        }
        for (const QStringList &l_group : groups) {
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

    // Every permission the gate can ask about, in declaration order and
    // without repeats - what /why walks to explain a command.
    QStringList permissions() const
    {
        QStringList l_names;
        for (const QStringList &l_group : groups) {
            for (const QString &l_permission : l_group) {
                if (!l_names.contains(l_permission)) {
                    l_names.append(l_permission);
                }
            }
        }
        return l_names;
    }

    // The words /help and /why print: "a, b+c" reads as a OR (b AND c).
    QString describe() const
    {
        if (groups.isEmpty()) {
            return QStringLiteral("nothing");
        }
        QStringList l_terms;
        for (const QStringList &l_group : groups) {
            l_terms << l_group.join(QLatin1Char('+'));
        }
        return l_terms.join(QStringLiteral(", "));
    }

    bool operator==(const Gate &) const = default;
};

// One gated form of a command. Variants let one command carry different
// permission requirements per argument shape, resolved at the dispatch gate
// before the handler runs - no permission check hides inside a handler body,
// so extension overrides and the command list see every gate.
struct CommandVariant
{
    QString id; // names the form for extension overrides ("command.id")
    int min_args = 0;
    int max_args = -1; // -1 accepts any count at or above min_args
    Gate gate;         // checked at the dispatch gate
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
    Gate gate;
    int min_args = 0;
    QString usage;
    QString description;
    int sensitive_args_from = -1;

    // When non-empty, dispatch picks the first variant whose argument count
    // matches and gates on ITS gate; the spec-level gate, min_args and
    // registered handler are not consulted.
    QList<CommandVariant> variants;

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
