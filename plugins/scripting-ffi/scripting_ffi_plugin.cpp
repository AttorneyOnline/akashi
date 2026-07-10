// AI-generated: written by Claude.
#include "scripting_ffi_plugin.h"

#include "akashi/config_entry.h"
#include "akashi/config_store.h"
#include "akashi/database_service.h"
#include "akashi/filesystem_service.h"
#include "akashi/logging_categories.h"
#include "akashi/scheduler.h"
#include "akashi/service_registry.h"
#include "core/command_context.h"
#include "core/command_registry.h"
#include "core/command_spec.h"
#include "core/console_menu.h"
#include "core/discord_hook.h"
#include "core/permission_registry.h"
#include "core/text_filter_registry.h"
#include "proto/packet_service.h"
#include "scripting_ffi_service.h"
#include "world/area.h"
#include "world/rule_registry.h"
#include "world/world.h"

#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QList>
#include <QMetaProperty>
#include <QSettings>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTime>
#include <QVarLengthArray>
#include <QVariantMap>

#include <functional>
#include <optional>
#include <vector>

// The C functions need reachable state; the plugin fills these on load and
// clears them on shutdown, after which every call turns into a no-op.
static akashi::CommandRegistry *s_commands = nullptr;
static akashi::TextFilterRegistry *s_filters = nullptr;
static akashi::PermissionRegistry *s_permissions = nullptr;
static akashi::ConfigStore *s_config = nullptr;
static akashi::RuleRegistry *s_rules = nullptr;
static akashi::ConsoleMenu *s_console = nullptr;
static akashi::FileSystemService *s_filesystem = nullptr;
static akashi::DatabaseService *s_databases = nullptr;
static akashi::DiscordHook *s_discord = nullptr;
static akashi::Scheduler *s_scheduler = nullptr;
static akashi::World *s_world = nullptr;
static akashi::PacketService *s_packets = nullptr;

// One in-progress Discord message per owner, driven by the discord_* verbs.
static QHash<QString, akashi::DiscordMessage> s_discord_drafts;

// The typed settings a script has declared, accumulated so each new
// config_declare re-declares the whole set.
static QHash<QString, QList<akashi::ConfigEntry>> s_declared_config;

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

// What an outbound interceptor writes its rewrite into.
struct AkashiPacketResult
{
    QString header;
    QStringList fields;
    bool set = false;
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

// Returns raw bytes, so a file read stays binary-safe.
static const char *bytesReturn(const QByteArray &f_bytes, size_t *f_out_length)
{
    s_string_slot = f_bytes;
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
    if (s_permissions) {
        s_permissions->unregisterAllPermissions(l_owner);
    }
    if (s_rules) {
        s_rules->unregisterActions(l_owner);
        s_rules->unregisterObservers(l_owner);
    }
    if (s_scheduler) {
        s_scheduler->cancelAll(l_owner);
    }
    if (s_packets) {
        s_packets->outboundInterceptors().unregisterAll(l_owner);
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

static int ffiSubscribeEvent(const char *f_name, size_t f_name_length,
                             AkashiEventFn f_handler, void *f_userdata,
                             const char *f_owner_id, size_t f_owner_id_length)
{
    if (!s_rules || !f_handler) {
        return 0;
    }
    QString l_name = toString(f_name, f_name_length);
    const QString l_owner = toString(f_owner_id, f_owner_id_length);

    // The script-facing names predate the unified event catalog; the old
    // aliases keep working by mapping onto the catalog ids they meant.
    static const QHash<QString, QString> s_aliases = {
        {QStringLiteral("player_joined_area"), QStringLiteral("player_joined")},
        {QStringLiteral("player_left_area"), QStringLiteral("player_left")},
        {QStringLiteral("ic_message"), QStringLiteral("ic_message_sent")},
        {QStringLiteral("ooc_message"), QStringLiteral("ooc_message_sent")},
    };
    l_name = s_aliases.value(l_name, l_name);

    // Every event goes through the one observer registry: core catalog
    // events, placeless events and custom cross-plugin names alike.
    s_rules->registerObserver(
        l_name, 0, [f_handler, f_userdata](const akashi::RuleContext &f_context) {
            // Scripts get one flat map: the payload plus the context ids,
            // named like a rule action sees them. Payload keys win.
            QVariantMap l_payload = f_context.payload;
            if (!l_payload.contains(QStringLiteral("player_state_id")))
                l_payload.insert(QStringLiteral("player_state_id"), f_context.player_state_id);
            if (!l_payload.contains(QStringLiteral("client_session_id")))
                l_payload.insert(QStringLiteral("client_session_id"), f_context.client_session_id);
            if (!l_payload.contains(QStringLiteral("area_id")))
                l_payload.insert(QStringLiteral("area_id"), f_context.area_id);
            if (!l_payload.contains(QStringLiteral("floor_id")))
                l_payload.insert(QStringLiteral("floor_id"), f_context.floor_id);
            deliverEvent(l_payload, f_handler, f_userdata);
        },
        l_owner);
    return 1;
}

static void ffiPublishEvent(const char *f_name, size_t f_name_length, int f_count,
                            const char *const *f_keys, const char *const *f_values)
{
    if (!s_rules) {
        return;
    }
    akashi::RuleContext l_context;
    for (int i = 0; i < f_count; i++) {
        l_context.payload.insert(QString::fromUtf8(f_keys[i]), QString::fromUtf8(f_values[i]));
    }
    s_rules->notifyObservers(toString(f_name, f_name_length), l_context);
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
    f_action(f_userdata, f_context.player_state_id, f_context.area_id, f_context.floor_id,
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
    const QString l_owner = toString(f_owner_id, f_owner_id_length);
    const QString l_key = toString(f_key, f_key_length);
    const QString l_name = QStringLiteral("plugins/") + l_owner;

    // A key present in the file wins; otherwise a declared setting supplies
    // its validated default; otherwise the caller's fallback.
    QSettings *l_settings = s_config->settings(l_name);
    if (l_settings && l_settings->contains(l_key)) {
        return stringReturn(l_settings->value(l_key).toString(), f_out_length);
    }
    for (const akashi::ConfigEntry &l_entry : std::as_const(s_declared_config[l_owner])) {
        if (l_entry.key() == l_key) {
            return stringReturn(s_config->value(l_name, l_key).toString(), f_out_length);
        }
    }
    return stringReturn(l_fallback, f_out_length);
}

static const char *ffiFsRead(const char *f_owner_id, size_t f_owner_id_length,
                             const char *f_path, size_t f_path_length,
                             size_t *f_out_length)
{
    if (!s_filesystem) {
        return stringReturn(QString(), f_out_length);
    }
    const auto l_resolved = s_filesystem->pluginResolve(toString(f_owner_id, f_owner_id_length),
                                                        toString(f_path, f_path_length));
    if (!l_resolved) {
        return stringReturn(QString(), f_out_length);
    }
    QFile l_file(*l_resolved);
    if (!l_file.open(QIODevice::ReadOnly)) {
        return stringReturn(QString(), f_out_length);
    }
    return bytesReturn(l_file.readAll(), f_out_length);
}

static int ffiFsWrite(const char *f_owner_id, size_t f_owner_id_length,
                      const char *f_path, size_t f_path_length,
                      const char *f_data, size_t f_data_length)
{
    if (!s_filesystem) {
        return 0;
    }
    const QString l_owner = toString(f_owner_id, f_owner_id_length);
    // Creating the base folder first also makes pluginResolve's boundary real.
    s_filesystem->pluginDataDir(l_owner);
    const auto l_resolved = s_filesystem->pluginResolve(l_owner, toString(f_path, f_path_length));
    if (!l_resolved) {
        return 0;
    }
    QDir().mkpath(QFileInfo(*l_resolved).absolutePath());
    const QByteArray l_data(f_data, int(f_data_length));
    return s_filesystem->writeFile(*l_resolved, l_data) ? 0 : 1;
}

static int ffiFsExists(const char *f_owner_id, size_t f_owner_id_length,
                       const char *f_path, size_t f_path_length)
{
    if (!s_filesystem) {
        return 0;
    }
    const auto l_resolved = s_filesystem->pluginResolve(toString(f_owner_id, f_owner_id_length),
                                                        toString(f_path, f_path_length));
    return (l_resolved && QFileInfo::exists(*l_resolved)) ? 1 : 0;
}

static int ffiConfigSet(const char *f_owner_id, size_t f_owner_id_length,
                        const char *f_key, size_t f_key_length,
                        const char *f_value, size_t f_value_length)
{
    if (!s_config) {
        return 0;
    }
    QSettings *l_settings = s_config->settings(QStringLiteral("plugins/") + toString(f_owner_id, f_owner_id_length));
    if (!l_settings) {
        return 0;
    }
    l_settings->setValue(toString(f_key, f_key_length), toString(f_value, f_value_length));
    l_settings->sync();
    return 1;
}

static void bindSqlParams(QSqlQuery &f_query, int f_param_count,
                          const char *const *f_params, const size_t *f_param_lengths)
{
    for (int i = 0; i < f_param_count; i++) {
        f_query.addBindValue(toString(f_params[i], f_param_lengths[i]));
    }
}

static int ffiSqlExec(const char *f_owner_id, size_t f_owner_id_length,
                      const char *f_sql, size_t f_sql_length,
                      int f_param_count, const char *const *f_params, const size_t *f_param_lengths)
{
    if (!s_databases) {
        return -1;
    }
    QSqlDatabase l_db = s_databases->pluginDatabase(toString(f_owner_id, f_owner_id_length));
    if (!l_db.isOpen()) {
        return -1;
    }
    QSqlQuery l_query(l_db);
    l_query.prepare(toString(f_sql, f_sql_length));
    bindSqlParams(l_query, f_param_count, f_params, f_param_lengths);
    if (!l_query.exec()) {
        qCWarning(akashiScripting) << "sql_exec failed:" << l_query.lastError().text();
        return -1;
    }
    return l_query.numRowsAffected();
}

// Runs a prepared select on a connection and streams each row to f_row.
// Shared by sql_query (own database) and sql_read (read-only databases).
static int runSelect(QSqlDatabase &f_db, const QString &f_sql,
                     int f_param_count, const char *const *f_params, const size_t *f_param_lengths,
                     AkashiSqlRowFn f_row, void *f_userdata)
{
    if (!f_db.isOpen()) {
        return -1;
    }
    QSqlQuery l_query(f_db);
    l_query.prepare(f_sql);
    bindSqlParams(l_query, f_param_count, f_params, f_param_lengths);
    if (!l_query.exec()) {
        qCWarning(akashiScripting) << "sql query failed:" << l_query.lastError().text();
        return -1;
    }

    int l_rows = 0;
    while (l_query.next()) {
        if (f_row) {
            const QSqlRecord l_record = l_query.record();
            const int l_columns = l_record.count();
            // Per-row storage kept alive across the callback, which may
            // re-enter the FFI.
            QList<QByteArray> l_names, l_values;
            l_names.reserve(l_columns);
            l_values.reserve(l_columns);
            for (int i = 0; i < l_columns; i++) {
                l_names.append(l_record.fieldName(i).toUtf8());
                l_values.append(l_query.value(i).toString().toUtf8());
            }
            QVarLengthArray<const char *> l_name_ptrs(l_columns), l_value_ptrs(l_columns);
            for (int i = 0; i < l_columns; i++) {
                l_name_ptrs[i] = l_names[i].constData();
                l_value_ptrs[i] = l_values[i].constData();
            }
            f_row(f_userdata, l_columns, l_name_ptrs.data(), l_value_ptrs.data());
        }
        l_rows++;
    }
    return l_rows;
}

static int ffiSqlQuery(const char *f_owner_id, size_t f_owner_id_length,
                       const char *f_sql, size_t f_sql_length,
                       int f_param_count, const char *const *f_params, const size_t *f_param_lengths,
                       AkashiSqlRowFn f_row, void *f_userdata)
{
    if (!s_databases) {
        return -1;
    }
    QSqlDatabase l_db = s_databases->pluginDatabase(toString(f_owner_id, f_owner_id_length));
    return runSelect(l_db, toString(f_sql, f_sql_length), f_param_count, f_params, f_param_lengths, f_row, f_userdata);
}

static void ffiDiscordBegin(const char *f_owner_id, size_t f_owner_id_length)
{
    s_discord_drafts[toString(f_owner_id, f_owner_id_length)] = akashi::DiscordMessage();
}

static void ffiDiscordSet(const char *f_owner_id, size_t f_owner_id_length,
                          const char *f_key, size_t f_key_length,
                          const char *f_value, size_t f_value_length)
{
    akashi::DiscordMessage &l_message = s_discord_drafts[toString(f_owner_id, f_owner_id_length)];
    const QString l_key = toString(f_key, f_key_length);
    const QString l_value = toString(f_value, f_value_length);
    if (l_key == QStringLiteral("content")) {
        l_message.setContent(l_value);
    }
    else if (l_key == QStringLiteral("username")) {
        l_message.setUsername(l_value);
    }
    else if (l_key == QStringLiteral("avatar_url")) {
        l_message.setAvatarUrl(l_value);
    }
    else if (l_key == QStringLiteral("tts")) {
        l_message.setTts(l_value == QStringLiteral("true") || l_value == QStringLiteral("1"));
    }
}

static void ffiDiscordEmbedBegin(const char *f_owner_id, size_t f_owner_id_length)
{
    s_discord_drafts[toString(f_owner_id, f_owner_id_length)].beginEmbed();
}

static void ffiDiscordEmbedSet(const char *f_owner_id, size_t f_owner_id_length,
                               const char *f_key, size_t f_key_length,
                               const char *f_value, size_t f_value_length)
{
    akashi::DiscordMessage &l_message = s_discord_drafts[toString(f_owner_id, f_owner_id_length)];
    const QString l_key = toString(f_key, f_key_length);
    const QString l_value = toString(f_value, f_value_length);
    if (l_key == QStringLiteral("title")) {
        l_message.setEmbedTitle(l_value);
    }
    else if (l_key == QStringLiteral("description")) {
        l_message.setEmbedDescription(l_value);
    }
    else if (l_key == QStringLiteral("url")) {
        l_message.setEmbedUrl(l_value);
    }
    else if (l_key == QStringLiteral("color")) {
        l_message.setEmbedColor(l_value);
    }
    else if (l_key == QStringLiteral("timestamp")) {
        l_message.setEmbedTimestamp(l_value);
    }
    else if (l_key == QStringLiteral("image")) {
        l_message.setEmbedImage(l_value);
    }
    else if (l_key == QStringLiteral("thumbnail")) {
        l_message.setEmbedThumbnail(l_value);
    }
}

static void ffiDiscordEmbedFooter(const char *f_owner_id, size_t f_owner_id_length,
                                  const char *f_text, size_t f_text_length,
                                  const char *f_icon_url, size_t f_icon_url_length)
{
    s_discord_drafts[toString(f_owner_id, f_owner_id_length)].setEmbedFooter(
        toString(f_text, f_text_length), toString(f_icon_url, f_icon_url_length));
}

static void ffiDiscordEmbedAuthor(const char *f_owner_id, size_t f_owner_id_length,
                                  const char *f_name, size_t f_name_length,
                                  const char *f_url, size_t f_url_length,
                                  const char *f_icon_url, size_t f_icon_url_length)
{
    s_discord_drafts[toString(f_owner_id, f_owner_id_length)].setEmbedAuthor(
        toString(f_name, f_name_length), toString(f_url, f_url_length), toString(f_icon_url, f_icon_url_length));
}

static void ffiDiscordEmbedField(const char *f_owner_id, size_t f_owner_id_length,
                                 const char *f_name, size_t f_name_length,
                                 const char *f_value, size_t f_value_length,
                                 int f_inline)
{
    s_discord_drafts[toString(f_owner_id, f_owner_id_length)].addEmbedField(
        toString(f_name, f_name_length), toString(f_value, f_value_length), f_inline != 0);
}

static void ffiDiscordEmbedEnd(const char *f_owner_id, size_t f_owner_id_length)
{
    s_discord_drafts[toString(f_owner_id, f_owner_id_length)].endEmbed();
}

static int ffiDiscordPost(const char *f_owner_id, size_t f_owner_id_length,
                          const char *f_url, size_t f_url_length)
{
    const QString l_owner = toString(f_owner_id, f_owner_id_length);
    const auto l_it = s_discord_drafts.find(l_owner);
    if (l_it == s_discord_drafts.end()) {
        return 0;
    }
    akashi::DiscordMessage l_message = l_it.value();
    s_discord_drafts.erase(l_it);
    if (!s_discord) {
        return 0;
    }
    l_message.setRequestUrl(toString(f_url, f_url_length));
    s_discord->post(l_message);
    return 1;
}

static int ffiConfigDeclare(const char *f_owner_id, size_t f_owner_id_length,
                            const char *f_key, size_t f_key_length,
                            const char *f_type, size_t f_type_length,
                            const char *f_default, size_t f_default_length,
                            const char *f_description, size_t f_description_length)
{
    if (!s_config) {
        return 0;
    }
    const QString l_owner = toString(f_owner_id, f_owner_id_length);
    const QString l_type = toString(f_type, f_type_length).toLower();
    const QString l_default_text = toString(f_default, f_default_length);

    // The declared type is the type of the default value; the config store
    // validates the file value against it at load.
    QVariant l_default;
    if (l_type == QStringLiteral("int")) {
        l_default = l_default_text.toInt();
    }
    else if (l_type == QStringLiteral("bool")) {
        l_default = l_default_text == QStringLiteral("true") || l_default_text == QStringLiteral("1");
    }
    else if (l_type == QStringLiteral("double")) {
        l_default = l_default_text.toDouble();
    }
    else {
        l_default = l_default_text;
    }

    QList<akashi::ConfigEntry> &l_entries = s_declared_config[l_owner];
    l_entries.removeIf([&](const akashi::ConfigEntry &f_entry) { return f_entry.key() == toString(f_key, f_key_length); });
    l_entries.append(akashi::ConfigEntry(toString(f_key, f_key_length), l_default, toString(f_description, f_description_length)));
    return s_config->declarePlugin(l_owner, l_entries) ? 1 : 0;
}

static int ffiSqlMigrate(const char *f_owner_id, size_t f_owner_id_length,
                         int f_to_version, AkashiMigrationFn f_migration, void *f_userdata)
{
    if (!s_databases || !f_migration) {
        return 0;
    }
    QSqlDatabase l_db = s_databases->pluginDatabase(toString(f_owner_id, f_owner_id_length));
    if (!l_db.isOpen()) {
        return 0;
    }
    if (akashi::DatabaseService::schemaVersion(l_db) >= f_to_version) {
        return 1;
    }
    // The migration body runs sql_exec statements on this same connection,
    // inside the transaction applyMigration opens; user_version bumps on commit.
    return akashi::DatabaseService::applyMigration(l_db, f_to_version, [f_migration, f_userdata](QSqlDatabase &) {
        return f_migration(f_userdata) != 0;
    })
               ? 1
               : 0;
}

static int ffiSqlRead(const char *f_source, size_t f_source_length,
                      const char *f_sql, size_t f_sql_length,
                      int f_param_count, const char *const *f_params, const size_t *f_param_lengths,
                      AkashiSqlRowFn f_row, void *f_userdata)
{
    if (!s_databases) {
        return -1;
    }
    QSqlDatabase l_db = s_databases->reader(toString(f_source, f_source_length));
    return runSelect(l_db, toString(f_sql, f_sql_length), f_param_count, f_params, f_param_lengths, f_row, f_userdata);
}

// Jobs are keyed by owner + id, so two plugins may use the same job id and
// each owner's jobs leave together on unload.
static QString scheduleId(const QString &f_owner, const QString &f_job_id)
{
    return f_owner + QStringLiteral(":") + f_job_id;
}

static QTime parseClock(const QString &f_time)
{
    QTime l_clock = QTime::fromString(f_time, QStringLiteral("HH:mm"));
    if (!l_clock.isValid()) {
        l_clock = QTime::fromString(f_time, QStringLiteral("H:mm"));
    }
    return l_clock;
}

static int ffiScheduleRepeating(const char *f_owner_id, size_t f_owner_id_length,
                                const char *f_job_id, size_t f_job_id_length,
                                const char *f_day, size_t f_day_length,
                                const char *f_time, size_t f_time_length,
                                AkashiJobFn f_action, void *f_userdata)
{
    if (!s_scheduler || !f_action) {
        return 0;
    }
    const QTime l_clock = parseClock(toString(f_time, f_time_length));
    if (!l_clock.isValid()) {
        return 0;
    }
    const akashi::Schedule l_schedule = akashi::Schedule::fromDayWord(toString(f_day, f_day_length), l_clock);
    const QString l_owner = toString(f_owner_id, f_owner_id_length);
    return s_scheduler->schedule(scheduleId(l_owner, toString(f_job_id, f_job_id_length)), l_schedule, [f_action, f_userdata] { f_action(f_userdata); }, l_owner) ? 1 : 0;
}

static int ffiScheduleOnce(const char *f_owner_id, size_t f_owner_id_length,
                           const char *f_job_id, size_t f_job_id_length,
                           const char *f_when, size_t f_when_length,
                           AkashiJobFn f_action, void *f_userdata)
{
    if (!s_scheduler || !f_action) {
        return 0;
    }
    const auto l_when = akashi::parseWhen(toString(f_when, f_when_length), QDateTime::currentDateTime());
    if (!l_when.has_value()) {
        return 0;
    }
    const QString l_owner = toString(f_owner_id, f_owner_id_length);
    return s_scheduler->schedule(scheduleId(l_owner, toString(f_job_id, f_job_id_length)), akashi::Schedule::once(*l_when), [f_action, f_userdata] { f_action(f_userdata); }, l_owner) ? 1 : 0;
}

static void ffiScheduleCancel(const char *f_owner_id, size_t f_owner_id_length,
                              const char *f_job_id, size_t f_job_id_length)
{
    if (s_scheduler) {
        s_scheduler->cancel(scheduleId(toString(f_owner_id, f_owner_id_length), toString(f_job_id, f_job_id_length)));
    }
}

static const char *ffiScheduleNextRun(const char *f_owner_id, size_t f_owner_id_length,
                                      const char *f_job_id, size_t f_job_id_length,
                                      size_t *f_out_length)
{
    if (!s_scheduler) {
        return stringReturn(QString(), f_out_length);
    }
    const auto l_next = s_scheduler->nextRunAt(scheduleId(toString(f_owner_id, f_owner_id_length),
                                                          toString(f_job_id, f_job_id_length)));
    return stringReturn(l_next.has_value() ? l_next->toString(QStringLiteral("yyyy-MM-dd hh:mm")) : QString(),
                        f_out_length);
}

// Area state crosses the ABI through Qt's property system, so a new area
// setting is one Q_PROPERTY line on Area with no change here: area_get reads
// any readable property, area_set writes any writable one.
static const char *ffiAreaGet(int f_area_id, const char *f_key, size_t f_key_length, size_t *f_out_length)
{
    if (!s_world) {
        return stringReturn(QString(), f_out_length);
    }
    akashi::Area *l_area = s_world->areaById(f_area_id);
    if (!l_area) {
        return stringReturn(QString(), f_out_length);
    }
    const QVariant l_value = l_area->property(toString(f_key, f_key_length).toUtf8().constData());
    if (!l_value.isValid()) {
        return stringReturn(QString(), f_out_length);
    }
    if (l_value.typeId() == QMetaType::Bool) {
        return stringReturn(l_value.toBool() ? QStringLiteral("true") : QStringLiteral("false"), f_out_length);
    }
    return stringReturn(l_value.toString(), f_out_length);
}

static int ffiAreaSet(int f_area_id, const char *f_key, size_t f_key_length,
                      const char *f_value, size_t f_value_length)
{
    if (!s_world) {
        return 0;
    }
    akashi::Area *l_area = s_world->areaById(f_area_id);
    if (!l_area) {
        return 0;
    }
    const QByteArray l_key = toString(f_key, f_key_length).toUtf8();
    const int l_index = l_area->metaObject()->indexOfProperty(l_key.constData());
    if (l_index < 0) {
        return 0;
    }
    const QMetaProperty l_property = l_area->metaObject()->property(l_index);
    if (!l_property.isWritable()) {
        return 0;
    }

    // Convert the string value to the property's type so a "false" bool is
    // really false, not a non-empty (truthy) string.
    const QString l_value = toString(f_value, f_value_length);
    QVariant l_typed;
    switch (l_property.typeId()) {
    case QMetaType::Bool:
        l_typed = l_value == QStringLiteral("true") || l_value == QStringLiteral("1");
        break;
    case QMetaType::Int:
        l_typed = l_value.toInt();
        break;
    default:
        l_typed = l_value;
        break;
    }
    return l_area->setProperty(l_key.constData(), l_typed) ? 1 : 0;
}

static const char *ffiFloorGet(int f_floor_id, const char *f_key, size_t f_key_length, size_t *f_out_length)
{
    if (!s_world) {
        return stringReturn(QString(), f_out_length);
    }
    const akashi::Floor *l_floor = s_world->floorById(f_floor_id);
    if (!l_floor) {
        return stringReturn(QString(), f_out_length);
    }
    if (toString(f_key, f_key_length) == QStringLiteral("name")) {
        return stringReturn(l_floor->name, f_out_length);
    }
    return stringReturn(QString(), f_out_length);
}

static int ffiWorldAreaCount()
{
    return s_world ? s_world->areaCount() : 0;
}

static int ffiWorldFloorCount()
{
    return s_world ? s_world->floorCount() : 0;
}

static int ffiRegisterOutboundInterceptor(const char *f_header, size_t f_header_length,
                                          int f_order,
                                          AkashiInterceptorFn f_interceptor, void *f_userdata,
                                          const char *f_owner_id, size_t f_owner_id_length)
{
    if (!s_packets || !f_interceptor) {
        return 0;
    }
    return s_packets->outboundInterceptors().registerInterceptor(
               toString(f_header, f_header_length), f_order,
               [f_interceptor, f_userdata](akashi::Packet &f_packet, akashi::IPacketContext &) {
                   const QStringList l_fields = f_packet.fields();
                   QList<QByteArray> l_field_utf8;
                   QVarLengthArray<const char *> l_field_ptrs(l_fields.size());
                   QVarLengthArray<size_t> l_field_lens(l_fields.size());
                   l_field_utf8.reserve(l_fields.size());
                   for (int i = 0; i < l_fields.size(); i++) {
                       l_field_utf8.append(l_fields[i].toUtf8());
                       l_field_ptrs[i] = l_field_utf8[i].constData();
                       l_field_lens[i] = size_t(l_field_utf8[i].size());
                   }
                   const QByteArray l_header = f_packet.header().toUtf8();

                   AkashiPacketResult l_result;
                   const int l_verdict = f_interceptor(f_userdata, l_header.constData(), size_t(l_header.size()),
                                                       l_fields.size(), l_field_ptrs.data(), l_field_lens.data(), &l_result);
                   if (l_result.set) {
                       f_packet.setHeader(l_result.header);
                       f_packet.setFields(l_result.fields);
                   }
                   return l_verdict ? akashi::PacketInterceptors::Verdict::Pass
                                    : akashi::PacketInterceptors::Verdict::Drop;
               },
               toString(f_owner_id, f_owner_id_length))
               ? 1
               : 0;
}

static void ffiPacketResultSet(AkashiPacketResult *f_result,
                               const char *f_header, size_t f_header_length,
                               int f_field_count, const char *const *f_fields, const size_t *f_field_lengths)
{
    if (!f_result) {
        return;
    }
    f_result->header = toString(f_header, f_header_length);
    f_result->fields.clear();
    for (int i = 0; i < f_field_count; i++) {
        f_result->fields.append(toString(f_fields[i], f_field_lengths[i]));
    }
    f_result->set = true;
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
    ffiFsRead,
    ffiFsWrite,
    ffiFsExists,
    ffiConfigSet,
    ffiSqlExec,
    ffiSqlQuery,
    ffiDiscordBegin,
    ffiDiscordSet,
    ffiDiscordEmbedBegin,
    ffiDiscordEmbedSet,
    ffiDiscordEmbedFooter,
    ffiDiscordEmbedAuthor,
    ffiDiscordEmbedField,
    ffiDiscordEmbedEnd,
    ffiDiscordPost,
    ffiConfigDeclare,
    ffiSqlMigrate,
    ffiSqlRead,
    ffiScheduleRepeating,
    ffiScheduleOnce,
    ffiScheduleCancel,
    ffiScheduleNextRun,
    ffiAreaGet,
    ffiAreaSet,
    ffiFloorGet,
    ffiWorldAreaCount,
    ffiWorldFloorCount,
    ffiRegisterOutboundInterceptor,
    ffiPacketResultSet,
};

namespace {

class ServiceImpl : public ScriptingFfiService
{
  public:
    const AkashiFfi *table() const override { return &s_table; }
};

} // namespace

QString ScriptingFfiPlugin::id() const { return QStringLiteral("akashi.scripting-ffi"); }
akashi::ServiceVersion ScriptingFfiPlugin::pluginVersion() const { return {1, 8, 0}; }

bool ScriptingFfiPlugin::load(akashi::ServiceRegistry &services)
{
    auto l_commands = services.resolve<akashi::CommandRegistry>(QStringLiteral("akashi.commands"));
    auto l_filters = services.resolve<akashi::TextFilterRegistry>(QStringLiteral("akashi.textfilters"));
    auto l_permissions = services.resolve<akashi::PermissionRegistry>(QStringLiteral("akashi.permissions"));
    auto l_config = services.resolve<akashi::ConfigStore>(QStringLiteral("akashi.config"));
    auto l_rules = services.resolve<akashi::RuleRegistry>(QStringLiteral("akashi.rules"));
    if (!l_commands || !l_filters || !l_permissions || !l_config || !l_rules) {
        qCWarning(akashiScripting) << "scripting-ffi: required services not available";
        return false;
    }
    s_commands = l_commands.get();
    s_filters = l_filters.get();
    s_permissions = l_permissions.get();
    s_config = l_config.get();
    s_rules = l_rules.get();
    // Optional: headless embeddings may run without a console menu.
    s_console = services.resolve<akashi::ConsoleMenu>(QStringLiteral("akashi.console")).get();
    // Optional too: the filesystem and database capabilities light up when
    // their services are present, and the fs_/sql_ verbs no-op without them.
    s_filesystem = services.resolve<akashi::FileSystemService>(QStringLiteral("akashi.filesystem")).get();
    s_databases = services.resolve<akashi::DatabaseService>(QStringLiteral("akashi.database")).get();
    s_discord = services.resolve<akashi::DiscordHook>(QStringLiteral("akashi.discordhook")).get();
    s_scheduler = services.resolve<akashi::Scheduler>(QStringLiteral("akashi.scheduler")).get();
    s_world = services.resolve<akashi::World>(QStringLiteral("akashi.world")).get();
    s_packets = services.resolve<akashi::PacketService>(QStringLiteral("akashi.packets")).get();

    m_service = std::make_shared<ServiceImpl>();
    if (!services.registerService(m_service, id())) {
        qCWarning(akashiScripting) << "scripting-ffi: service id already taken";
        s_commands = nullptr;
        s_filters = nullptr;
        s_permissions = nullptr;
        s_config = nullptr;
        s_rules = nullptr;
        s_console = nullptr;
        s_filesystem = nullptr;
        s_databases = nullptr;
        s_discord = nullptr;
        s_scheduler = nullptr;
        s_world = nullptr;
        s_packets = nullptr;
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
    s_permissions = nullptr;
    s_config = nullptr;
    s_rules = nullptr;
    s_console = nullptr;
    s_filesystem = nullptr;
    s_databases = nullptr;
    s_discord = nullptr;
    s_scheduler = nullptr;
    s_world = nullptr;
    s_packets = nullptr;
    s_discord_drafts.clear();
    s_declared_config.clear();
    m_service.reset();
}
