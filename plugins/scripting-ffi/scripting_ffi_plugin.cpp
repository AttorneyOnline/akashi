// AI-generated: written by Claude.
#include "scripting_ffi_plugin.h"

#include "akashi/config_store.h"
#include "akashi/event.h"
#include "akashi/logging_categories.h"
#include "akashi/service_registry.h"
#include "core/command_context.h"
#include "core/command_registry.h"
#include "core/command_spec.h"
#include "core/console_menu.h"
#include "core/event_bus.h"
#include "core/permission_registry.h"
#include "core/text_filter_registry.h"
#include "scripting_ffi_service.h"
#include "world/rule_registry.h"

#include <QByteArray>
#include <QDebug>
#include <QHash>
#include <QList>
#include <QSettings>
#include <QVariantMap>

#include <functional>
#include <optional>
#include <vector>

// The C functions need reachable state; the plugin fills these on load and
// clears them on shutdown, after which every call turns into a no-op.
static akashi::CommandRegistry *s_commands = nullptr;
static akashi::TextFilterRegistry *s_filters = nullptr;
static akashi::EventBus *s_events = nullptr;
static akashi::PermissionRegistry *s_permissions = nullptr;
static akashi::ConfigStore *s_config = nullptr;
static akashi::RuleRegistry *s_rules = nullptr;
static akashi::ConsoleMenu *s_console = nullptr;

// The slot string returns point into; valid until the next FFI call.
static QByteArray s_string_slot;

// What a text filter callback writes its rewrite into.
struct AkashiTextResult
{
    QString text;
    bool set = false;
};

// What a before-rule callback writes its refusal into.
struct AkashiRuleResult
{
    QString reason;
    bool blocked = false;
};

static QString toString(const char *f_text, size_t f_length)
{
    return f_text ? QString::fromUtf8(f_text, int(f_length)) : QString();
}

static const char *stringReturn(const QString &f_value, size_t *f_out_length)
{
    s_string_slot = f_value.toUtf8();
    if (f_out_length) {
        *f_out_length = size_t(s_string_slot.size());
    }
    return s_string_slot.constData();
}

static akashi::CommandContext *context(AkashiCommandContext *f_context)
{
    return reinterpret_cast<akashi::CommandContext *>(f_context);
}

static void ffiLogInfo(const char *f_text, size_t f_text_length)
{
    qCInfo(akashiScripting).noquote() << toString(f_text, f_text_length);
}

static int ffiRegisterCommand(const char *f_name, size_t f_name_length,
                              const char *f_usage, size_t f_usage_length,
                              const char *f_description, size_t f_description_length,
                              const char *f_permission, size_t f_permission_length,
                              int f_min_args,
                              AkashiCommandFn f_handler, void *f_userdata,
                              const char *f_owner_id, size_t f_owner_id_length)
{
    if (!s_commands || !f_handler) {
        return 0;
    }

    akashi::CommandSpec l_spec;
    l_spec.name = toString(f_name, f_name_length);
    l_spec.usage = toString(f_usage, f_usage_length);
    l_spec.description = toString(f_description, f_description_length);
    l_spec.min_args = f_min_args;
    const QString l_permission = toString(f_permission, f_permission_length);
    if (!l_permission.isEmpty()) {
        l_spec.permissions = {l_permission};
    }
    if (l_spec.name.isEmpty()) {
        return 0;
    }

    const bool l_registered = s_commands->registerCommand(
        l_spec, [f_handler, f_userdata](akashi::CommandContext &f_context) {
        // The UTF-8 copies stay alive for the duration of the callback.
        QList<QByteArray> l_utf8;
        std::vector<const char *> l_argv;
        const QStringList l_arguments = f_context.arguments();
        l_utf8.reserve(l_arguments.size());
        l_argv.reserve(l_arguments.size());
        for (const QString &l_argument : l_arguments) {
            l_utf8.append(l_argument.toUtf8());
            l_argv.push_back(l_utf8.last().constData());
        }
        f_handler(f_userdata, reinterpret_cast<AkashiCommandContext *>(&f_context),
                  int(l_argv.size()), l_argv.data()); }, toString(f_owner_id, f_owner_id_length));
    return l_registered ? 1 : 0;
}

static void ffiUnregisterOwner(const char *f_owner_id, size_t f_owner_id_length)
{
    const QString l_owner = toString(f_owner_id, f_owner_id_length);
    if (s_commands) {
        s_commands->unregisterAll(l_owner);
    }
    if (s_filters) {
        s_filters->unregisterAll(l_owner);
    }
    if (s_events) {
        s_events->unsubscribeAll(l_owner);
    }
    if (s_permissions) {
        s_permissions->unregisterAllPermissions(l_owner);
    }
    if (s_rules) {
        s_rules->unregisterActions(l_owner);
    }
}

static void ffiReply(AkashiCommandContext *f_context, const char *f_text, size_t f_text_length)
{
    if (f_context) {
        context(f_context)->reply(toString(f_text, f_text_length));
    }
}

static void ffiReplyToArea(AkashiCommandContext *f_context, const char *f_text, size_t f_text_length)
{
    if (f_context) {
        context(f_context)->replyToArea(toString(f_text, f_text_length));
    }
}

static int ffiClientId(AkashiCommandContext *f_context)
{
    return f_context ? context(f_context)->clientId() : -1;
}

static const char *ffiContextPlayerName(AkashiCommandContext *f_context, size_t *f_out_length)
{
    return stringReturn(f_context ? context(f_context)->name() : QString(), f_out_length);
}

static const char *ffiContextCharacter(AkashiCommandContext *f_context, size_t *f_out_length)
{
    return stringReturn(f_context ? context(f_context)->character() : QString(), f_out_length);
}

static const char *ffiContextAreaName(AkashiCommandContext *f_context, size_t *f_out_length)
{
    return stringReturn(f_context ? context(f_context)->areaName() : QString(), f_out_length);
}

static int ffiContextAreaId(AkashiCommandContext *f_context)
{
    return f_context ? context(f_context)->areaId() : -1;
}

static int ffiContextIsAuthenticated(AkashiCommandContext *f_context)
{
    return f_context && context(f_context)->isAuthenticated() ? 1 : 0;
}

static int ffiContextCanPerform(AkashiCommandContext *f_context,
                                const char *f_permission, size_t f_permission_length)
{
    return f_context && context(f_context)->canPerform(toString(f_permission, f_permission_length)) ? 1 : 0;
}

static std::optional<akashi::TargetPlayer> target(AkashiCommandContext *f_context, int f_argument_index)
{
    if (!f_context) {
        return std::nullopt;
    }
    return context(f_context)->resolveTarget(f_argument_index);
}

static int ffiTargetClientId(AkashiCommandContext *f_context, int f_argument_index)
{
    auto l_target = target(f_context, f_argument_index);
    return l_target ? l_target->clientId() : -1;
}

static int ffiTargetReply(AkashiCommandContext *f_context, int f_argument_index,
                          const char *f_text, size_t f_text_length)
{
    auto l_target = target(f_context, f_argument_index);
    if (!l_target) {
        return 0;
    }
    l_target->reply(toString(f_text, f_text_length));
    return 1;
}

static int ffiTargetHasSanction(AkashiCommandContext *f_context, int f_argument_index,
                                const char *f_sanction_id, size_t f_sanction_id_length)
{
    auto l_target = target(f_context, f_argument_index);
    return l_target && l_target->hasSanction(toString(f_sanction_id, f_sanction_id_length)) ? 1 : 0;
}

static int ffiTargetSetSanction(AkashiCommandContext *f_context, int f_argument_index,
                                const char *f_sanction_id, size_t f_sanction_id_length,
                                int f_active)
{
    auto l_target = target(f_context, f_argument_index);
    if (!l_target) {
        return 0;
    }
    l_target->setSanction(toString(f_sanction_id, f_sanction_id_length), f_active != 0);
    return 1;
}

static int ffiTargetChangeArea(AkashiCommandContext *f_context, int f_argument_index, int f_area_id)
{
    auto l_target = target(f_context, f_argument_index);
    if (!l_target) {
        return 0;
    }
    l_target->changeArea(f_area_id);
    return 1;
}

static int ffiRegisterTextFilter(const char *f_id, size_t f_id_length,
                                 int f_order, int f_always_active,
                                 AkashiTextFilterFn f_filter, void *f_userdata,
                                 const char *f_owner_id, size_t f_owner_id_length)
{
    const QString l_id = toString(f_id, f_id_length);
    if (!s_filters || !f_filter || l_id.isEmpty() || s_filters->hasFilter(l_id)) {
        return 0;
    }
    s_filters->registerFilter(
        l_id, f_order, [f_filter, f_userdata](const QString &f_text) -> std::optional<QString> {
        const QByteArray l_utf8 = f_text.toUtf8();
        AkashiTextResult l_result;
        if (!f_filter(f_userdata, l_utf8.constData(), size_t(l_utf8.size()), &l_result)) {
            return std::nullopt;
        }
        return l_result.set ? l_result.text : f_text; }, f_always_active != 0, toString(f_owner_id, f_owner_id_length));
    return 1;
}

static void ffiTextResultSet(AkashiTextResult *f_result, const char *f_text, size_t f_text_length)
{
    if (f_result) {
        f_result->text = toString(f_text, f_text_length);
        f_result->set = true;
    }
}

// Hands one event payload to a script handler as key/value string pairs.
static void deliverEvent(const QVariantMap &f_payload, AkashiEventFn f_handler, void *f_userdata)
{
    QList<QByteArray> l_keys, l_values;
    std::vector<const char *> l_key_ptrs, l_value_ptrs;
    l_keys.reserve(f_payload.size());
    l_values.reserve(f_payload.size());
    for (auto it = f_payload.begin(); it != f_payload.end(); ++it) {
        l_keys.append(it.key().toUtf8());
        l_values.append(it.value().toString().toUtf8());
        l_key_ptrs.push_back(l_keys.last().constData());
        l_value_ptrs.push_back(l_values.last().constData());
    }
    f_handler(f_userdata, int(l_key_ptrs.size()), l_key_ptrs.data(), l_value_ptrs.data());
}

static QVariantMap toMap(const akashi::PlayerJoinedAreaEvent &e)
{
    return {{"client_id", e.client_id}, {"area_id", e.area_id}, {"floor_id", e.floor_id}, {"char_name", e.char_name}, {"ipid", e.ipid}};
}
static QVariantMap toMap(const akashi::PlayerLeftAreaEvent &e)
{
    return {{"client_id", e.client_id}, {"area_id", e.area_id}, {"floor_id", e.floor_id}, {"char_name", e.char_name}};
}
static QVariantMap toMap(const akashi::AreaChangedEvent &e)
{
    return {{"client_id", e.client_id}, {"from_area", e.from_area}, {"to_area", e.to_area}, {"char_name", e.char_name}};
}
static QVariantMap toMap(const akashi::ICMessageEvent &e)
{
    return {{"client_id", e.client_id}, {"area_id", e.area_id}, {"floor_id", e.floor_id}, {"area_name", e.area_name}, {"char_name", e.char_name}, {"ooc_name", e.ooc_name}, {"ipid", e.ipid}, {"message", e.message}};
}
static QVariantMap toMap(const akashi::OOCMessageEvent &e)
{
    return {{"client_id", e.client_id}, {"area_id", e.area_id}, {"area_name", e.area_name}, {"char_name", e.char_name}, {"ooc_name", e.ooc_name}, {"ipid", e.ipid}, {"message", e.message}};
}
static QVariantMap toMap(const akashi::MusicChangedEvent &e)
{
    return {{"client_id", e.client_id}, {"area_id", e.area_id}, {"floor_id", e.floor_id}, {"area_name", e.area_name}, {"char_name", e.char_name}, {"track_name", e.track_name}};
}
static QVariantMap toMap(const akashi::EvidencePresentedEvent &e)
{
    return {{"client_id", e.client_id}, {"area_id", e.area_id}, {"floor_id", e.floor_id}, {"char_name", e.char_name}, {"evidence_name", e.evidence_name}};
}
static QVariantMap toMap(const akashi::ModcallEvent &e)
{
    return {{"client_id", e.client_id}, {"area_id", e.area_id}, {"area_name", e.area_name}, {"char_name", e.char_name}, {"ooc_name", e.ooc_name}, {"ipid", e.ipid}, {"reason", e.reason}};
}
static QVariantMap toMap(const akashi::BanIssuedEvent &e)
{
    return {{"ban_id", e.ban_id}, {"moderator", e.moderator}, {"target_ipid", e.target_ipid}, {"duration", e.duration}, {"reason", e.reason}};
}
static QVariantMap toMap(const akashi::KickIssuedEvent &e)
{
    return {{"moderator", e.moderator}, {"target_ipid", e.target_ipid}, {"reason", e.reason}};
}
static QVariantMap toMap(const akashi::CommandExecutedEvent &e)
{
    return {{"client_id", e.client_id}, {"area_id", e.area_id}, {"char_name", e.char_name}, {"ipid", e.ipid}, {"command", e.command}, {"args", e.args}};
}
static QVariantMap toMap(const akashi::ConfigReloadedEvent &)
{
    return {};
}

template <typename E>
static void subscribeCoreEvent(AkashiEventFn f_handler, void *f_userdata, const QString &f_owner)
{
    s_events->subscribe<E>(
        akashi::EventPhase::After, 0, [f_handler, f_userdata](E &f_event) { deliverEvent(toMap(f_event), f_handler, f_userdata); }, f_owner);
}

static int ffiSubscribeEvent(const char *f_name, size_t f_name_length,
                             AkashiEventFn f_handler, void *f_userdata,
                             const char *f_owner_id, size_t f_owner_id_length)
{
    if (!s_events || !f_handler) {
        return 0;
    }
    const QString l_name = toString(f_name, f_name_length);
    const QString l_owner = toString(f_owner_id, f_owner_id_length);

    using Registrar = std::function<void(AkashiEventFn, void *, const QString &)>;
    static const QHash<QString, Registrar> s_core_events = {
        {QStringLiteral("player_joined_area"), subscribeCoreEvent<akashi::PlayerJoinedAreaEvent>},
        {QStringLiteral("player_left_area"), subscribeCoreEvent<akashi::PlayerLeftAreaEvent>},
        {QStringLiteral("area_changed"), subscribeCoreEvent<akashi::AreaChangedEvent>},
        {QStringLiteral("ic_message"), subscribeCoreEvent<akashi::ICMessageEvent>},
        {QStringLiteral("ooc_message"), subscribeCoreEvent<akashi::OOCMessageEvent>},
        {QStringLiteral("music_changed"), subscribeCoreEvent<akashi::MusicChangedEvent>},
        {QStringLiteral("evidence_presented"), subscribeCoreEvent<akashi::EvidencePresentedEvent>},
        {QStringLiteral("modcall"), subscribeCoreEvent<akashi::ModcallEvent>},
        {QStringLiteral("ban_issued"), subscribeCoreEvent<akashi::BanIssuedEvent>},
        {QStringLiteral("kick_issued"), subscribeCoreEvent<akashi::KickIssuedEvent>},
        {QStringLiteral("command_executed"), subscribeCoreEvent<akashi::CommandExecutedEvent>},
        {QStringLiteral("config_reloaded"), subscribeCoreEvent<akashi::ConfigReloadedEvent>},
    };

    if (auto it = s_core_events.constFind(l_name); it != s_core_events.constEnd()) {
        (*it)(f_handler, f_userdata, l_owner);
        return 1;
    }

    s_events->subscribeCustom(
        l_name, akashi::EventPhase::After, [f_handler, f_userdata](const QVariantMap &f_payload) { deliverEvent(f_payload, f_handler, f_userdata); }, l_owner);
    return 1;
}

static void ffiPublishEvent(const char *f_name, size_t f_name_length, int f_count,
                            const char *const *f_keys, const char *const *f_values)
{
    if (!s_events) {
        return;
    }
    QVariantMap l_payload;
    for (int i = 0; i < f_count; i++) {
        l_payload.insert(QString::fromUtf8(f_keys[i]), QString::fromUtf8(f_values[i]));
    }
    s_events->publishCustom(toString(f_name, f_name_length), l_payload);
}

static int ffiRegisterPermission(const char *f_id, size_t f_id_length,
                                 const char *f_display_name, size_t f_display_name_length,
                                 const char *f_category, size_t f_category_length,
                                 const char *f_owner_id, size_t f_owner_id_length)
{
    if (!s_permissions) {
        return 0;
    }
    akashi::PermissionInfo l_info;
    l_info.id = toString(f_id, f_id_length);
    l_info.display_name = toString(f_display_name, f_display_name_length);
    l_info.category = toString(f_category, f_category_length);
    if (l_info.id.isEmpty()) {
        return 0;
    }
    return s_permissions->registerPermission(l_info, toString(f_owner_id, f_owner_id_length)) ? 1 : 0;
}

// Marshals one rule fire into the C callback: the context ids plus the
// event payload and the attached arguments as key/value pairs.
static void callRuleAction(AkashiRuleFn f_action, void *f_userdata,
                           const akashi::RuleContext &f_context, const QVariantMap &f_args,
                           AkashiRuleResult *f_result)
{
    QList<QByteArray> l_payload_keys, l_payload_values, l_arg_keys, l_arg_values;
    std::vector<const char *> l_pk, l_pv, l_ak, l_av;
    for (auto it = f_context.payload.begin(); it != f_context.payload.end(); ++it) {
        l_payload_keys.append(it.key().toUtf8());
        l_payload_values.append(it.value().toString().toUtf8());
        l_pk.push_back(l_payload_keys.last().constData());
        l_pv.push_back(l_payload_values.last().constData());
    }
    for (auto it = f_args.begin(); it != f_args.end(); ++it) {
        l_arg_keys.append(it.key().toUtf8());
        l_arg_values.append(it.value().toString().toUtf8());
        l_ak.push_back(l_arg_keys.last().constData());
        l_av.push_back(l_arg_values.last().constData());
    }
    f_action(f_userdata, f_context.player_id, f_context.area_id, f_context.floor_id,
             int(l_pk.size()), l_pk.data(), l_pv.data(),
             int(l_ak.size()), l_ak.data(), l_av.data(), f_result);
}

static int ffiRegisterRuleAction(const char *f_name, size_t f_name_length, int f_before,
                                 AkashiRuleFn f_action, void *f_userdata,
                                 const char *f_owner_id, size_t f_owner_id_length)
{
    const QString l_name = toString(f_name, f_name_length);
    if (!s_rules || !f_action || l_name.isEmpty() || s_rules->hasAction(l_name)) {
        return 0;
    }
    const QString l_owner = toString(f_owner_id, f_owner_id_length);

    if (f_before) {
        s_rules->registerBeforeAction(
            l_name, [f_action, f_userdata](akashi::ServiceRegistry &, const QVariantMap &f_args) -> akashi::BeforeRuleFunction { return [f_action, f_userdata, f_args](const akashi::RuleContext &f_context) -> akashi::RuleVerdict {
                                                                                                                                     AkashiRuleResult l_result;
                                                                                                                                     callRuleAction(f_action, f_userdata, f_context, f_args, &l_result);
                                                                                                                                     if (!l_result.blocked) {
                                                                                                                                         return {true, {}};
                                                                                                                                     }
                                                                                                                                     return {false, l_result.reason.isEmpty() ? QStringLiteral("This is not allowed here.") : l_result.reason};
                                                                                                                                 }; }, l_owner);
    }
    else {
        s_rules->registerAfterAction(
            l_name, [f_action, f_userdata](akashi::ServiceRegistry &, const QVariantMap &f_args) -> akashi::AfterRuleFunction { return [f_action, f_userdata, f_args](const akashi::RuleContext &f_context) {
                                                                                                                                    callRuleAction(f_action, f_userdata, f_context, f_args, nullptr);
                                                                                                                                }; }, l_owner);
    }
    return 1;
}

static void ffiRuleResultBlock(AkashiRuleResult *f_result, const char *f_reason, size_t f_reason_length)
{
    if (f_result) {
        f_result->reason = toString(f_reason, f_reason_length);
        f_result->blocked = true;
    }
}

static int ffiRegisterConsoleAction(const char *f_title, size_t f_title_length,
                                    AkashiConsoleFn f_action, void *f_userdata,
                                    const char *f_owner_id, size_t f_owner_id_length)
{
    if (!s_console || !f_action) {
        return 0;
    }
    return s_console->registerAction(
               toString(f_title, f_title_length), [f_action, f_userdata] { f_action(f_userdata); }, toString(f_owner_id, f_owner_id_length))
               ? 1
               : 0;
}

static void ffiConsolePrint(const char *f_text, size_t f_text_length)
{
    const QString l_text = toString(f_text, f_text_length);
    if (akashi::ConsoleMenu *l_session = akashi::ConsoleMenu::activeSession()) {
        l_session->printOut(l_text);
        return;
    }
    qCInfo(akashiConsole).noquote() << l_text;
}

static const char *ffiConfigGet(const char *f_owner_id, size_t f_owner_id_length,
                                const char *f_key, size_t f_key_length,
                                const char *f_fallback, size_t f_fallback_length,
                                size_t *f_out_length)
{
    const QString l_fallback = toString(f_fallback, f_fallback_length);
    if (!s_config) {
        return stringReturn(l_fallback, f_out_length);
    }
    QSettings *l_settings = s_config->settings(QStringLiteral("plugins/") + toString(f_owner_id, f_owner_id_length));
    const QString l_value = l_settings ? l_settings->value(toString(f_key, f_key_length), l_fallback).toString() : l_fallback;
    return stringReturn(l_value, f_out_length);
}

static const AkashiFfi s_table = {
    AKASHI_FFI_ABI_VERSION,
    ffiLogInfo,
    ffiRegisterCommand,
    ffiUnregisterOwner,
    ffiReply,
    ffiReplyToArea,
    ffiClientId,
    ffiContextPlayerName,
    ffiContextCharacter,
    ffiContextAreaName,
    ffiContextAreaId,
    ffiContextIsAuthenticated,
    ffiContextCanPerform,
    ffiTargetClientId,
    ffiTargetReply,
    ffiTargetHasSanction,
    ffiTargetSetSanction,
    ffiTargetChangeArea,
    ffiRegisterTextFilter,
    ffiTextResultSet,
    ffiSubscribeEvent,
    ffiPublishEvent,
    ffiRegisterPermission,
    ffiConfigGet,
    ffiRegisterRuleAction,
    ffiRuleResultBlock,
    ffiRegisterConsoleAction,
    ffiConsolePrint,
};

namespace {

class ServiceImpl : public ScriptingFfiService
{
  public:
    const AkashiFfi *table() const override { return &s_table; }
};

} // namespace

QString ScriptingFfiPlugin::id() const { return QStringLiteral("akashi.scripting-ffi"); }
akashi::ServiceVersion ScriptingFfiPlugin::pluginVersion() const { return {1, 3, 0}; }

bool ScriptingFfiPlugin::load(akashi::ServiceRegistry &services)
{
    auto l_commands = services.resolve<akashi::CommandRegistry>(QStringLiteral("akashi.commands"));
    auto l_filters = services.resolve<akashi::TextFilterRegistry>(QStringLiteral("akashi.textfilters"));
    auto l_events = services.resolve<akashi::EventBus>(QStringLiteral("akashi.events"));
    auto l_permissions = services.resolve<akashi::PermissionRegistry>(QStringLiteral("akashi.permissions"));
    auto l_config = services.resolve<akashi::ConfigStore>(QStringLiteral("akashi.config"));
    auto l_rules = services.resolve<akashi::RuleRegistry>(QStringLiteral("akashi.rules"));
    if (!l_commands || !l_filters || !l_events || !l_permissions || !l_config || !l_rules) {
        qCWarning(akashiScripting) << "scripting-ffi: required services not available";
        return false;
    }
    s_commands = l_commands.get();
    s_filters = l_filters.get();
    s_events = l_events.get();
    s_permissions = l_permissions.get();
    s_config = l_config.get();
    s_rules = l_rules.get();
    // Optional: headless embeddings may run without a console menu.
    s_console = services.resolve<akashi::ConsoleMenu>(QStringLiteral("akashi.console")).get();

    m_service = std::make_shared<ServiceImpl>();
    if (!services.registerService(m_service, id())) {
        qCWarning(akashiScripting) << "scripting-ffi: service id already taken";
        s_commands = nullptr;
        s_filters = nullptr;
        s_events = nullptr;
        s_permissions = nullptr;
        s_config = nullptr;
        return false;
    }

    qCInfo(akashiScripting) << "scripting-ffi: C surface available, ABI version" << AKASHI_FFI_ABI_VERSION;
    return true;
}

void ScriptingFfiPlugin::shutdown(akashi::ServiceRegistry &services)
{
    services.unregisterService(QStringLiteral("akashi.scripting-ffi"));
    s_commands = nullptr;
    s_filters = nullptr;
    s_events = nullptr;
    s_permissions = nullptr;
    s_config = nullptr;
    s_rules = nullptr;
    s_console = nullptr;
    m_service.reset();
}
