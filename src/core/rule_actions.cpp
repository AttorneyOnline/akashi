#include "core/rule_actions.h"

#include "akashi/logging_categories.h"
#include "core/client_session.h"
#include "core/server_context.h"
#include "proto/text_utils.h"
#include "world/area.h"
#include "world/jukebox.h"
#include "world/rule_registry.h"

#include <QTime>
#include <QTimer>

#include <functional>

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

// The boolean area settings a check_setting rule may gate on.
using SettingGetter = bool (Area::*)() const;
const QHash<QString, SettingGetter> &settingGetters()
{
    static const QHash<QString, SettingGetter> s_getters = {
        {QStringLiteral("blankposting"), &Area::isBlankpostingAllowed},
        {QStringLiteral("iniswap"), &Area::isIniswapAllowed},
        {QStringLiteral("music"), &Area::isMusicAllowed},
        {QStringLiteral("ic_messages"), &Area::isMessageAllowed},
        {QStringLiteral("wtce"), &Area::isWtceAllowed},
        {QStringLiteral("shouts"), &Area::isShoutAllowed},
        {QStringLiteral("shownames"), &Area::isShownameAllowed},
    };
    return s_getters;
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

// Refuses entry when the payload's character is already taken there.
BeforeRuleFunction checkCharacter(ServerContext *f_server, ServiceRegistry &, const QVariantMap &f_args)
{
    QString l_policy = f_args.value(QStringLiteral("policy"), QStringLiteral("no_duplicates")).toString();
    return [f_server, l_policy](const RuleContext &f_ctx) -> RuleVerdict {
        if (l_policy != QStringLiteral("no_duplicates"))
            return {};
        int l_char_id = f_ctx.payload.contains(QStringLiteral("character_id"))
                            ? f_ctx.payload.value(QStringLiteral("character_id")).toInt()
                            : -1;
        if (l_char_id < 0)
            return {};
        akashi::Area *l_area = areaFor(f_server, f_ctx);
        if (l_area && l_area->charactersTaken().contains(l_char_id))
            return {false, QStringLiteral("That character is already taken in ") + f_server->areaName(f_ctx.area_id) + QStringLiteral(".")};
        return {};
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
// bypass permission.
BeforeRuleFunction checkSetting(ServerContext *f_server, ServiceRegistry &, const QVariantMap &f_args)
{
    QString l_setting = f_args.value(QStringLiteral("setting")).toString();
    QString l_message = f_args.value(QStringLiteral("message"), QStringLiteral("That is disabled in this area.")).toString();
    QString l_bypass = f_args.value(QStringLiteral("bypass")).toString();
    if (!settingGetters().contains(l_setting)) {
        qCWarning(akashiConfig) << "check_setting rule references unknown setting" << l_setting << "- the rule will never block.";
    }
    return [f_server, l_setting, l_message, l_bypass](const RuleContext &f_ctx) -> RuleVerdict {
        akashi::Area *l_area = areaFor(f_server, f_ctx);
        SettingGetter l_getter = settingGetters().value(l_setting, nullptr);
        if (!l_area || !l_getter)
            return {};
        if (std::invoke(l_getter, l_area))
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
                l_client->sendPacket("TI", {QString::number(l_timer_id), "2"});
                l_client->sendPacket("TI", {QString::number(l_timer_id), "0", QString::number(QTime(0, 0).msecsTo(QTime(0, 0).addMSecs(l_timer->remainingTime())))});
            }
            else {
                l_client->sendPacket("TI", {QString::number(l_timer_id), "3"});
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

constexpr GateDef s_gates[] = {
    {"block", &block},
    {"check_lock", &checkLock},
    {"check_character", &checkCharacter},
    {"check_permission", &checkPermission},
    {"check_setting", &checkSetting},
    {"check_blankposting", &checkBlankposting},
    {"check_iniswap", &checkIniswap},
    {"check_evidence_access", &checkEvidenceAccess},
    {"check_background", &checkBackground},
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
    for (const ReactionDef &l_reaction : s_reactions) {
        f_registry->registerAfterAction(QLatin1String(l_reaction.name), std::bind_front(l_reaction.build, f_server), l_owner);
    }
}

} // namespace akashi
