#include "core/rule_actions.h"

#include "akashi/logging_categories.h"
#include "akashi/service_registry.h"
#include "core/client_session.h"
#include "core/server_context.h"
#include "core/text_filter_registry.h"
#include "proto/text_utils.h"
#include "proto/timer_packets.h"
#include "world/area.h"
#include "world/jukebox.h"
#include "world/rule_registry.h"
#include "world/world.h"

#include <QMetaObject>
#include <QMetaProperty>
#include <QTime>
#include <QTimer>

#include <functional>
#include <memory>

namespace akashi {

namespace {

// Every action resolves the acting client the same way; a null client means
// the event fired without a player attached, so gates pass and sends no-op.
akashi::ClientSession *clientFor(ServerContext *f_server, const RuleContext &f_context)
{
    return f_server->clientById(f_context.player_id);
}

akashi::Area *areaFor(ServerContext *f_server, const RuleContext &f_context)
{
    return f_server->areaById(f_context.area_id);
}

// --- Gate factories. Each builds one before-rule from its arguments. ---

// The unconditional "no".
BeforeRuleFunction block(ServerContext *, ServiceRegistry &, const QVariantMap &f_args)
{
    QString l_message = f_args.value(QStringLiteral("message"), QStringLiteral("You cannot do that here.")).toString();
    return [l_message](const RuleContext &) -> RuleVerdict {
        return {false, l_message};
    };
}

// Holds the door of a locked area. honor_invites=false ignores the guest
// list; the bypass_locks permission always wins.
BeforeRuleFunction checkLock(ServerContext *, ServiceRegistry &, const QVariantMap &f_args)
{
    bool l_honor_invites = f_args.value(QStringLiteral("honor_invites"), true).toBool();
    return [l_honor_invites](const RuleContext &f_ctx) -> RuleVerdict {
        if (f_ctx.payload.value(QStringLiteral("lock_status")).toString() != QStringLiteral("locked"))
            return {};
        if (l_honor_invites && f_ctx.payload.value(QStringLiteral("is_invited")).toBool())
            return {};
        // No rule argument may switch a granted permission off.
        if (f_ctx.payload.value(QStringLiteral("bypass_locks")).toBool())
            return {};
        return {false, QStringLiteral("Area ") + f_ctx.payload.value(QStringLiteral("area_name")).toString() + QStringLiteral(" is locked.")};
    };
}

// Blocks claiming a character that is already held. The policy argument
// picks the scope: unique_per_area mirrors the per-area mechanism (so the
// default rule changes nothing, and bypass_rules holders still meet the
// mechanism's refusal below the rules), unique_on_floor holds a claim
// against every area on the acting client's floor. Without a message
// argument the refusal is silent, like the mechanism's always has been;
// the legacy no_duplicates policy (the player_joined shape) keeps its
// spoken taken-in-area default. Both events' payloads carry character_id.
// The world resolves through the service registry, so the policy runs
// wherever the world lives, servers and tests alike.
BeforeRuleFunction checkCharacter(ServerContext *, ServiceRegistry &f_services, const QVariantMap &f_args)
{
    std::shared_ptr<World> l_world = f_services.resolve<World>(QStringLiteral("akashi.world"));
    if (!l_world) {
        qCWarning(akashiConfig) << "check_character rule finds no world service - the rule will never block.";
    }
    QString l_policy = f_args.value(QStringLiteral("policy"), QStringLiteral("no_duplicates")).toString();
    bool l_has_message = f_args.contains(QStringLiteral("message"));
    QString l_message = f_args.value(QStringLiteral("message")).toString();
    return [l_world, l_policy, l_has_message, l_message](const RuleContext &f_ctx) -> RuleVerdict {
        int l_char_id = f_ctx.payload.contains(QStringLiteral("character_id"))
                            ? f_ctx.payload.value(QStringLiteral("character_id")).toInt()
                            : -1;
        // A switch to spectator claims nothing.
        if (!l_world || l_char_id < 0)
            return {};
        bool l_taken = false;
        if (l_policy == QStringLiteral("unique_on_floor")) {
            const Floor *l_floor = l_world->floorById(f_ctx.floor_id);
            const QVector<int> l_area_ids = l_floor ? l_floor->area_ids : QVector<int>{};
            for (int l_area_id : l_area_ids) {
                Area *l_area = l_world->areaById(l_area_id);
                if (l_area && l_area->charactersTaken().contains(l_char_id)) {
                    l_taken = true;
                }
            }
        }
        else {
            Area *l_area = l_world->areaById(f_ctx.area_id);
            l_taken = l_area && l_area->charactersTaken().contains(l_char_id);
        }
        if (!l_taken)
            return {};
        if (l_has_message)
            return {false, l_message};
        if (l_policy == QStringLiteral("no_duplicates"))
            return {false, QStringLiteral("That character is already taken in ") + l_world->areaName(f_ctx.area_id) + QStringLiteral(".")};
        return {false, QString()};
    };
}

// Requires a named permission.
BeforeRuleFunction checkPermission(ServerContext *f_server, ServiceRegistry &, const QVariantMap &f_args)
{
    QString l_permission = f_args.value(QStringLiteral("permission")).toString();
    QString l_message = f_args.value(QStringLiteral("message"), QStringLiteral("You do not have permission to do that here.")).toString();
    return [f_server, l_permission, l_message](const RuleContext &f_ctx) -> RuleVerdict {
        akashi::ClientSession *l_client = clientFor(f_server, f_ctx);
        if (!l_client || l_permission.isEmpty())
            return {};
        if (!l_client->canPerform(l_permission))
            return {false, l_message};
        return {};
    };
}

// Blocks while a boolean area setting is off, unless the player holds the
// bypass permission. The setting vocabulary is Area's Q_PROPERTY surface
// (music_allowed, wtce_allowed, ...), read through the metaobject, so a
// new area knob is gateable with zero code here.
BeforeRuleFunction checkSetting(ServerContext *f_server, ServiceRegistry &, const QVariantMap &f_args)
{
    QString l_setting = f_args.value(QStringLiteral("setting")).toString();
    QString l_message = f_args.value(QStringLiteral("message"), QStringLiteral("That is disabled in this area.")).toString();
    QString l_bypass = f_args.value(QStringLiteral("bypass")).toString();
    const int l_property_index = Area::staticMetaObject.indexOfProperty(l_setting.toUtf8().constData());
    if (l_property_index < 0) {
        qCWarning(akashiConfig) << "check_setting rule references unknown setting" << l_setting << "- the rule will never block.";
    }
    return [f_server, l_message, l_bypass, l_property_index](const RuleContext &f_ctx) -> RuleVerdict {
        akashi::Area *l_area = areaFor(f_server, f_ctx);
        if (!l_area || l_property_index < 0)
            return {};
        if (Area::staticMetaObject.property(l_property_index).read(l_area).toBool())
            return {};
        akashi::ClientSession *l_client = clientFor(f_server, f_ctx);
        if (!l_bypass.isEmpty() && l_client && l_client->canPerform(l_bypass))
            return {};
        return {false, l_message};
    };
}

// Refuses blank IC messages where the area forbids them.
BeforeRuleFunction checkBlankposting(ServerContext *f_server, ServiceRegistry &, const QVariantMap &)
{
    return [f_server](const RuleContext &f_ctx) -> RuleVerdict {
        akashi::Area *l_area = areaFor(f_server, f_ctx);
        if (!l_area || l_area->isBlankpostingAllowed())
            return {};
        if (!f_ctx.payload.contains(QStringLiteral("message")))
            return {};
        const QString l_text = stripZalgo(f_ctx.payload.value(QStringLiteral("message")).toString().trimmed());
        if (l_text.isEmpty())
            return {false, QStringLiteral("Blankposting has been forbidden in this area.")};
        return {};
    };
}

// Refuses unknown character folders where the area forbids iniswapping;
// swapping to a listed character stays allowed, as it always was.
BeforeRuleFunction checkIniswap(ServerContext *f_server, ServiceRegistry &, const QVariantMap &)
{
    return [f_server](const RuleContext &f_ctx) -> RuleVerdict {
        akashi::Area *l_area = areaFor(f_server, f_ctx);
        akashi::ClientSession *l_client = clientFor(f_server, f_ctx);
        if (!l_area || !l_client || l_area->isIniswapAllowed())
            return {};
        const QString l_char_name = f_ctx.payload.value(QStringLiteral("char_name")).toString();
        if (l_char_name.isEmpty() || l_char_name.compare(l_client->character(), Qt::CaseInsensitive) == 0)
            return {};
        const QStringList l_split = l_char_name.split(QStringLiteral("/"));
        if (!f_server->characters().contains(l_split.at(0), Qt::CaseInsensitive) || l_split.contains(QStringLiteral("..")))
            return {false, QStringLiteral("Iniswapping is not allowed in this area.")};
        return {};
    };
}

// Refuses a custom showname where the area forbids them. Payload-dependent,
// so it is its own action rather than a check_setting; the handler keeps
// the length and whitespace hygiene.
BeforeRuleFunction checkShowname(ServerContext *f_server, ServiceRegistry &, const QVariantMap &)
{
    return [f_server](const RuleContext &f_ctx) -> RuleVerdict {
        akashi::Area *l_area = areaFor(f_server, f_ctx);
        akashi::ClientSession *l_client = clientFor(f_server, f_ctx);
        if (!l_area || !l_client || l_area->isShownameAllowed())
            return {};
        const QString l_showname = stripZalgo(f_ctx.payload.value(QStringLiteral("showname")).toString().trimmed());
        if (l_showname.isEmpty() || l_showname == l_client->character())
            return {};
        return {false, QStringLiteral("Shownames are not allowed in this area!")};
    };
}

// Enforces the area's evidence mod (FFA/CM/mod).
BeforeRuleFunction checkEvidenceAccess(ServerContext *f_server, ServiceRegistry &, const QVariantMap &)
{
    return [f_server](const RuleContext &f_ctx) -> RuleVerdict {
        akashi::ClientSession *l_client = clientFor(f_server, f_ctx);
        if (l_client && !l_client->canModifyEvidence())
            return {false, QStringLiteral("You are not allowed to modify the evidence here.")};
        return {};
    };
}

// Holds a locked background against everyone but moderators.
BeforeRuleFunction checkBackground(ServerContext *f_server, ServiceRegistry &, const QVariantMap &)
{
    return [f_server](const RuleContext &f_ctx) -> RuleVerdict {
        akashi::Area *l_area = areaFor(f_server, f_ctx);
        akashi::ClientSession *l_client = clientFor(f_server, f_ctx);
        if (l_area && l_area->isBgLocked() && l_client && !l_client->isAuthenticated())
            return {false, QStringLiteral("This area's background is locked.")};
        return {};
    };
}

// Blocks free /play-style music or ambience (payload source "play") while
// the area's play command is off, unless the player owns the area or holds
// the bypass permission.
BeforeRuleFunction checkFreePlay(ServerContext *f_server, ServiceRegistry &, const QVariantMap &f_args)
{
    QString l_message = f_args.value(QStringLiteral("message"), QStringLiteral("Free music play is disabled in this area.")).toString();
    QString l_bypass = f_args.value(QStringLiteral("bypass")).toString();
    return [f_server, l_message, l_bypass](const RuleContext &f_ctx) -> RuleVerdict {
        if (f_ctx.payload.value(QStringLiteral("source")).toString() != QStringLiteral("play"))
            return {};
        akashi::Area *l_area = areaFor(f_server, f_ctx);
        if (!l_area || l_area->isPlayEnabled())
            return {};
        if (l_area->owners().contains(f_ctx.player_id))
            return {};
        akashi::ClientSession *l_client = clientFor(f_server, f_ctx);
        if (!l_bypass.isEmpty() && l_client && l_client->canPerform(l_bypass))
            return {};
        return {false, l_message};
    };
}

// Blocks WT/CE splashes (payload kind "wtce") while the area's wtce setting
// is off. Penalties (kind "penalty") were never gated by the setting and
// pass through.
BeforeRuleFunction checkWtce(ServerContext *f_server, ServiceRegistry &, const QVariantMap &f_args)
{
    QString l_message = f_args.value(QStringLiteral("message"), QStringLiteral("WTCE animations have been disabled in this area.")).toString();
    QString l_bypass = f_args.value(QStringLiteral("bypass")).toString();
    return [f_server, l_message, l_bypass](const RuleContext &f_ctx) -> RuleVerdict {
        if (f_ctx.payload.value(QStringLiteral("kind")).toString() != QStringLiteral("wtce"))
            return {};
        akashi::Area *l_area = areaFor(f_server, f_ctx);
        if (!l_area || l_area->isWtceAllowed())
            return {};
        akashi::ClientSession *l_client = clientFor(f_server, f_ctx);
        if (!l_bypass.isEmpty() && l_client && l_client->canPerform(l_bypass))
            return {};
        return {false, l_message};
    };
}

// --- Transform factories. Each builds one payload-rewriting rule. ---
// Transforms run for everyone: bypass_rules skips gates, not area flavor.

// Downgrades shouts to plain messages where the area's shout knob is off.
// The protocol hygiene (custom "4", [0,4] range) stays with the handler.
TransformRuleFunction stripShouts(ServerContext *f_server, ServiceRegistry &, const QVariantMap &)
{
    return [f_server](const RuleContext &f_ctx) -> QVariantMap {
        akashi::Area *l_area = areaFor(f_server, f_ctx);
        if (!l_area || l_area->isShoutAllowed())
            return {};
        if (f_ctx.payload.value(QStringLiteral("objection_mod")).toString() == QStringLiteral("0"))
            return {};
        akashi::ClientSession *l_client = clientFor(f_server, f_ctx);
        if (l_client)
            l_client->sendServerMessage(QStringLiteral("Shouts have been disabled in this area."));
        return {{QStringLiteral("objection_mod"), QStringLiteral("0")}};
    };
}

// Runs one named text filter over the payload's message. Transforms
// rewrite, they never gate: a filter that drops leaves the message alone.
TransformRuleFunction applyFilter(ServerContext *f_server, ServiceRegistry &, const QVariantMap &f_args)
{
    QString l_filter = f_args.value(QStringLiteral("filter")).toString();
    if (l_filter.isEmpty()) {
        qCWarning(akashiConfig) << "apply_filter rule names no filter - the rule will never rewrite.";
    }
    return [f_server, l_filter](const RuleContext &f_ctx) -> QVariantMap {
        if (l_filter.isEmpty() || !f_ctx.payload.contains(QStringLiteral("message")))
            return {};
        const auto l_result = f_server->textFilterRegistry()->applyFilter(l_filter, f_ctx.payload.value(QStringLiteral("message")).toString());
        if (!l_result)
            return {};
        return {{QStringLiteral("message"), *l_result}};
    };
}

// Medieval-izes the message while the area's medieval knob is on. Dedicated
// knob-reading action, same shape as check_blankposting, so /medievalmode
// keeps working with no rule edits.
TransformRuleFunction applyMedieval(ServerContext *f_server, ServiceRegistry &, const QVariantMap &)
{
    return [f_server](const RuleContext &f_ctx) -> QVariantMap {
        akashi::Area *l_area = areaFor(f_server, f_ctx);
        if (!l_area || !l_area->isMedievalMode() || !f_ctx.payload.contains(QStringLiteral("message")))
            return {};
        const auto l_result = f_server->textFilterRegistry()->applyFilter(QStringLiteral("medieval"), f_ctx.payload.value(QStringLiteral("message")).toString());
        if (!l_result)
            return {};
        return {{QStringLiteral("message"), *l_result}};
    };
}

// --- Reaction factories. Each builds one after-rule. ---

// A server OOC message to the player, or to the whole area.
AfterRuleFunction sendMessage(ServerContext *f_server, ServiceRegistry &, const QVariantMap &f_args)
{
    QString l_message = f_args.value(QStringLiteral("message")).toString();
    bool l_to_area = f_args.value(QStringLiteral("target")).toString() == QStringLiteral("area");
    return [f_server, l_message, l_to_area](const RuleContext &f_ctx) {
        akashi::ClientSession *l_client = clientFor(f_server, f_ctx);
        if (!l_client || l_message.isEmpty())
            return;
        if (l_to_area)
            l_client->sendServerMessageArea(l_message);
        else
            l_client->sendServerMessage(l_message);
    };
}

AfterRuleFunction sendEvidenceList(ServerContext *f_server, ServiceRegistry &, const QVariantMap &)
{
    return [f_server](const RuleContext &f_ctx) {
        akashi::ClientSession *l_client = clientFor(f_server, f_ctx);
        akashi::Area *l_area = areaFor(f_server, f_ctx);
        if (l_client && l_area)
            l_client->sendEvidenceList(l_area);
    };
}

AfterRuleFunction sendPenalties(ServerContext *f_server, ServiceRegistry &, const QVariantMap &)
{
    return [f_server](const RuleContext &f_ctx) {
        akashi::ClientSession *l_client = clientFor(f_server, f_ctx);
        akashi::Area *l_area = areaFor(f_server, f_ctx);
        if (!l_client || !l_area)
            return;
        l_client->sendPacket("HP", {"1", QString::number(l_area->defHP())});
        l_client->sendPacket("HP", {"2", QString::number(l_area->proHP())});
    };
}

AfterRuleFunction sendBackground(ServerContext *f_server, ServiceRegistry &, const QVariantMap &)
{
    return [f_server](const RuleContext &f_ctx) {
        akashi::ClientSession *l_client = clientFor(f_server, f_ctx);
        akashi::Area *l_area = areaFor(f_server, f_ctx);
        if (l_client && l_area)
            l_client->sendPacket("BN", {l_area->background(), l_area->side()});
    };
}

AfterRuleFunction sendTimers(ServerContext *f_server, ServiceRegistry &, const QVariantMap &)
{
    return [f_server](const RuleContext &f_ctx) {
        akashi::ClientSession *l_client = clientFor(f_server, f_ctx);
        akashi::Area *l_area = areaFor(f_server, f_ctx);
        if (!l_client || !l_area)
            return;
        const QList<QTimer *> l_timers = l_area->timers();
        for (QTimer *l_timer : l_timers) {
            int l_timer_id = l_timers.indexOf(l_timer) + 1;
            if (l_timer->isActive()) {
                l_client->sendPacket(timerShow(l_timer_id));
                l_client->sendPacket(timerValue(l_timer_id, QTime(0, 0).msecsTo(QTime(0, 0).addMSecs(l_timer->remainingTime()))));
            }
            else {
                l_client->sendPacket(timerHide(l_timer_id));
            }
        }
    };
}

// The music list, the ambience, and the current song.
AfterRuleFunction sendSong(ServerContext *f_server, ServiceRegistry &, const QVariantMap &)
{
    return [f_server](const RuleContext &f_ctx) {
        akashi::ClientSession *l_client = clientFor(f_server, f_ctx);
        akashi::Area *l_area = areaFor(f_server, f_ctx);
        if (!l_client || !l_area)
            return;
        l_client->sendPacket(akashi::Packet("FM", l_area->jukebox()->resolvedList()));
        l_client->sendPacket("MC", {l_area->currentAmbience(), QString::number(-1), f_server->serverNickname(), QString::number(1), QString::number(1)});
        l_client->sendPacket("MC", {l_area->currentMusic(), QString::number(-1), f_server->serverNickname(), QString::number(1)});
    };
}

// The floor's area list and a full ARUP, only when the floor changed.
AfterRuleFunction sendFloorAreas(ServerContext *f_server, ServiceRegistry &, const QVariantMap &)
{
    return [f_server](const RuleContext &f_ctx) {
        int l_from_floor = f_ctx.payload.value(QStringLiteral("from_floor"), -1).toInt();
        if (l_from_floor == f_ctx.floor_id)
            return;
        akashi::ClientSession *l_client = clientFor(f_server, f_ctx);
        if (!l_client)
            return;
        l_client->sendPacket(akashi::Packet("FA", l_client->floorAreaNames()));
        l_client->sendFullArup();
    };
}

AfterRuleFunction sendAreaDescription(ServerContext *f_server, ServiceRegistry &, const QVariantMap &)
{
    return [f_server](const RuleContext &f_ctx) {
        akashi::ClientSession *l_client = clientFor(f_server, f_ctx);
        akashi::Area *l_area = areaFor(f_server, f_ctx);
        if (l_client && l_area && l_area->sendAreaMessageOnJoin())
            l_client->sendServerMessage(l_area->areaMessage());
    };
}

// Leaving an area takes the invitation with it, unless the player owns the
// area they left.
AfterRuleFunction revokeInvite(ServerContext *f_server, ServiceRegistry &, const QVariantMap &)
{
    return [f_server](const RuleContext &f_ctx) {
        const int l_from = f_ctx.payload.value(QStringLiteral("from_area"), -1).toInt();
        akashi::Area *l_area = f_server->areaById(l_from);
        if (l_area && !l_area->owners().contains(f_ctx.player_id))
            l_area->uninvite(f_ctx.player_id);
    };
}

AfterRuleFunction sendLockNotice(ServerContext *f_server, ServiceRegistry &, const QVariantMap &)
{
    return [f_server](const RuleContext &f_ctx) {
        akashi::ClientSession *l_client = clientFor(f_server, f_ctx);
        akashi::Area *l_area = areaFor(f_server, f_ctx);
        if (l_client && l_area && l_area->lockState() == akashi::Area::LockState::Spectatable)
            l_client->sendServerMessage(QStringLiteral("Area ") + f_server->areaName(f_ctx.area_id) + QStringLiteral(" is spectate-only; to chat IC you will need to be invited by the CM."));
    };
}

// --- The catalog. Registration walks these tables. ---

using BeforeFactory = BeforeRuleFunction (*)(ServerContext *, ServiceRegistry &, const QVariantMap &);
using AfterFactory = AfterRuleFunction (*)(ServerContext *, ServiceRegistry &, const QVariantMap &);
using TransformFactory = TransformRuleFunction (*)(ServerContext *, ServiceRegistry &, const QVariantMap &);

struct GateDef
{
    const char *name;
    BeforeFactory build;
};

struct ReactionDef
{
    const char *name;
    AfterFactory build;
};

struct TransformDef
{
    const char *name;
    TransformFactory build;
};

constexpr GateDef s_gates[] = {
    {"block", &block},
    {"check_lock", &checkLock},
    {"check_character", &checkCharacter},
    {"check_permission", &checkPermission},
    {"check_setting", &checkSetting},
    {"check_blankposting", &checkBlankposting},
    {"check_iniswap", &checkIniswap},
    {"check_showname", &checkShowname},
    {"check_evidence_access", &checkEvidenceAccess},
    {"check_background", &checkBackground},
    {"check_free_play", &checkFreePlay},
    {"check_wtce", &checkWtce},
};

constexpr TransformDef s_transforms[] = {
    {"strip_shouts", &stripShouts},
    {"apply_filter", &applyFilter},
    {"apply_medieval", &applyMedieval},
};

constexpr ReactionDef s_reactions[] = {
    {"send_message", &sendMessage},
    {"send_evidence_list", &sendEvidenceList},
    {"send_penalties", &sendPenalties},
    {"send_background", &sendBackground},
    {"send_timers", &sendTimers},
    {"send_song", &sendSong},
    {"send_floor_areas", &sendFloorAreas},
    {"send_area_description", &sendAreaDescription},
    {"send_lock_notice", &sendLockNotice},
    {"revoke_invite", &revokeInvite},
};

} // namespace

void registerCoreRuleActions(ServerContext *f_server, RuleRegistry *f_registry)
{
    const QString l_owner = QStringLiteral("core");
    for (const GateDef &l_gate : s_gates) {
        f_registry->registerBeforeAction(QLatin1String(l_gate.name), std::bind_front(l_gate.build, f_server), l_owner);
    }
    for (const TransformDef &l_transform : s_transforms) {
        f_registry->registerTransformAction(QLatin1String(l_transform.name), std::bind_front(l_transform.build, f_server), l_owner);
    }
    for (const ReactionDef &l_reaction : s_reactions) {
        f_registry->registerAfterAction(QLatin1String(l_reaction.name), std::bind_front(l_reaction.build, f_server), l_owner);
    }
}

} // namespace akashi
