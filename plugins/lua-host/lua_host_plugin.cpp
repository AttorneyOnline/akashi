#include "lua_host_plugin.h"

#include "akashi/logging_categories.h"
#include "akashi/script_plugin_host.h"
#include "akashi/service_registry.h"
#include "akashi_ffi.h"
#include "core/plugin_manager.h"
#include "scripting_ffi_service.h"

#include <QByteArray>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QList>

#include <cstring>

extern "C"
{
#include "lua/lauxlib.h"
#include "lua/lua.h"
#include "lua/lualib.h"
}

// One registered Lua function: the state it lives in and a registry
// reference to it. Owned by the host, released with its plugin.
struct LuaFnRef
{
    lua_State *state = nullptr;
    int function_ref = LUA_NOREF;
};

// Everything one Lua plugin owns: its interpreter and its handlers.
struct LuaPluginState
{
    lua_State *state = nullptr;
    QByteArray owner_id;
    QList<LuaFnRef *> fn_refs;
};

static const AkashiFfi *s_ffi = nullptr;
static QHash<QString, LuaPluginState *> s_plugins;

// The plugin state a lua_State belongs to, stashed in its registry.
static LuaPluginState *pluginOf(lua_State *L)
{
    lua_getfield(L, LUA_REGISTRYINDEX, "akashi.plugin");
    auto l_plugin = static_cast<LuaPluginState *>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return l_plugin;
}

static LuaFnRef *takeFnRef(lua_State *L, int f_stack_index)
{
    LuaPluginState *l_plugin = pluginOf(L);
    if (!l_plugin) {
        return nullptr;
    }
    auto l_ref = new LuaFnRef;
    l_ref->state = L;
    lua_pushvalue(L, f_stack_index);
    l_ref->function_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    l_plugin->fn_refs.append(l_ref);
    return l_ref;
}

// Runs a registered Lua handler as (context, args) with args as an array.
static void luaCommandTrampoline(void *f_userdata, AkashiCommandContext *f_context,
                                 int f_argc, const char *const *f_argv)
{
    LuaFnRef *l_ref = static_cast<LuaFnRef *>(f_userdata);
    lua_State *L = l_ref->state;
    lua_rawgeti(L, LUA_REGISTRYINDEX, l_ref->function_ref);
    lua_pushlightuserdata(L, f_context);
    lua_createtable(L, f_argc, 0);
    for (int i = 0; i < f_argc; i++) {
        lua_pushstring(L, f_argv[i]);
        lua_rawseti(L, -2, i + 1);
    }
    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        qCWarning(akashiScripting).noquote() << "lua-host: handler error:" << lua_tostring(L, -1);
        lua_pop(L, 1);
    }
}

// Runs a Lua text filter: a returned string rewrites the message, false
// drops it, anything else leaves it unchanged.
static int luaFilterTrampoline(void *f_userdata, const char *f_text, size_t f_text_length,
                               AkashiTextResult *f_result)
{
    LuaFnRef *l_ref = static_cast<LuaFnRef *>(f_userdata);
    lua_State *L = l_ref->state;
    lua_rawgeti(L, LUA_REGISTRYINDEX, l_ref->function_ref);
    lua_pushlstring(L, f_text, f_text_length);
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        qCWarning(akashiScripting).noquote() << "lua-host: filter error:" << lua_tostring(L, -1);
        lua_pop(L, 1);
        return 1;
    }
    int l_keep = 1;
    if (lua_isstring(L, -1)) {
        size_t l_length = 0;
        const char *l_rewritten = lua_tolstring(L, -1, &l_length);
        s_ffi->text_result_set(f_result, l_rewritten, l_length);
    }
    else if (lua_isboolean(L, -1) && !lua_toboolean(L, -1)) {
        l_keep = 0;
    }
    lua_pop(L, 1);
    return l_keep;
}

// Runs a Lua rule action with one info table: ids, the event payload and
// the attached arguments. A returned string blocks with that reason; false
// blocks with the stock reason; anything else allows.
static void luaRuleTrampoline(void *f_userdata,
                              int f_player_state_id, int f_area_id, int f_floor_id,
                              int f_payload_count, const char *const *f_payload_keys, const char *const *f_payload_values,
                              int f_argument_count, const char *const *f_argument_keys, const char *const *f_argument_values,
                              AkashiRuleResult *f_result)
{
    LuaFnRef *l_ref = static_cast<LuaFnRef *>(f_userdata);
    lua_State *L = l_ref->state;
    lua_rawgeti(L, LUA_REGISTRYINDEX, l_ref->function_ref);
    lua_createtable(L, 0, 5);
    lua_pushinteger(L, f_player_state_id);
    lua_setfield(L, -2, "player_state_id");
    lua_pushinteger(L, f_area_id);
    lua_setfield(L, -2, "area_id");
    lua_pushinteger(L, f_floor_id);
    lua_setfield(L, -2, "floor_id");
    lua_createtable(L, 0, f_payload_count);
    for (int i = 0; i < f_payload_count; i++) {
        lua_pushstring(L, f_payload_values[i]);
        lua_setfield(L, -2, f_payload_keys[i]);
    }
    lua_setfield(L, -2, "payload");
    lua_createtable(L, 0, f_argument_count);
    for (int i = 0; i < f_argument_count; i++) {
        lua_pushstring(L, f_argument_values[i]);
        lua_setfield(L, -2, f_argument_keys[i]);
    }
    lua_setfield(L, -2, "args");

    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        qCWarning(akashiScripting).noquote() << "lua-host: rule action error:" << lua_tostring(L, -1);
        lua_pop(L, 1);
        return;
    }
    if (f_result) {
        if (lua_isstring(L, -1)) {
            size_t l_length = 0;
            const char *l_reason = lua_tolstring(L, -1, &l_length);
            s_ffi->rule_result_block(f_result, l_reason, l_length);
        }
        else if (lua_isboolean(L, -1) && !lua_toboolean(L, -1)) {
            s_ffi->rule_result_block(f_result, "", 0);
        }
    }
    lua_pop(L, 1);
}

// Runs a Lua event handler with the payload as a table.
static void luaEventTrampoline(void *f_userdata, int f_count,
                               const char *const *f_keys, const char *const *f_values)
{
    LuaFnRef *l_ref = static_cast<LuaFnRef *>(f_userdata);
    lua_State *L = l_ref->state;
    lua_rawgeti(L, LUA_REGISTRYINDEX, l_ref->function_ref);
    lua_createtable(L, 0, f_count);
    for (int i = 0; i < f_count; i++) {
        lua_pushstring(L, f_values[i]);
        lua_setfield(L, -2, f_keys[i]);
    }
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        qCWarning(akashiScripting).noquote() << "lua-host: event handler error:" << lua_tostring(L, -1);
        lua_pop(L, 1);
    }
}

// Runs a registered Lua console task.
static void luaConsoleTrampoline(void *f_userdata)
{
    LuaFnRef *l_ref = static_cast<LuaFnRef *>(f_userdata);
    lua_State *L = l_ref->state;
    lua_rawgeti(L, LUA_REGISTRYINDEX, l_ref->function_ref);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        qCWarning(akashiScripting).noquote() << "lua-host: console task error:" << lua_tostring(L, -1);
        lua_pop(L, 1);
    }
}

static int luaApiRegisterConsoleAction(lua_State *L)
{
    size_t l_title_length = 0;
    const char *l_title = luaL_checklstring(L, 1, &l_title_length);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    LuaPluginState *l_plugin = pluginOf(L);
    LuaFnRef *l_ref = takeFnRef(L, 2);
    if (!l_plugin || !l_ref) {
        return luaL_error(L, "register_console_action: no plugin is attached to this state");
    }
    lua_pushboolean(L, s_ffi->register_console_action(l_title, l_title_length, luaConsoleTrampoline, l_ref,
                                                      l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size())));
    return 1;
}

static int luaApiLog(lua_State *L)
{
    size_t l_length = 0;
    const char *l_text = luaL_checklstring(L, 1, &l_length);
    s_ffi->log_info(l_text, l_length);
    return 0;
}

static int luaApiConsolePrint(lua_State *L)
{
    size_t l_length = 0;
    const char *l_text = luaL_checklstring(L, 1, &l_length);
    // An older core's table ends before console_print; fall back to the log.
    if (s_ffi->abi_version >= 5) {
        s_ffi->console_print(l_text, l_length);
    }
    else {
        s_ffi->log_info(l_text, l_length);
    }
    return 0;
}

static int luaApiRegisterCommand(lua_State *L)
{
    size_t l_name_length = 0, l_usage_length = 0, l_description_length = 0, l_permission_length = 0;
    const char *l_name = luaL_checklstring(L, 1, &l_name_length);
    const char *l_usage = luaL_checklstring(L, 2, &l_usage_length);
    const char *l_description = luaL_checklstring(L, 3, &l_description_length);
    luaL_checktype(L, 4, LUA_TFUNCTION);
    const char *l_permission = luaL_optlstring(L, 5, "", &l_permission_length);
    const int l_min_args = int(luaL_optinteger(L, 6, 0));

    LuaPluginState *l_plugin = pluginOf(L);
    LuaFnRef *l_ref = takeFnRef(L, 4);
    if (!l_plugin || !l_ref) {
        return luaL_error(L, "register_command: no plugin is attached to this state");
    }

    const int l_registered = s_ffi->register_command(
        l_name, l_name_length, l_usage, l_usage_length,
        l_description, l_description_length,
        l_permission, l_permission_length, l_min_args,
        luaCommandTrampoline, l_ref,
        l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()));
    if (!l_registered) {
        return luaL_error(L, "register_command: the name is taken or invalid");
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int luaApiRegisterTextFilter(lua_State *L)
{
    size_t l_id_length = 0;
    const char *l_id = luaL_checklstring(L, 1, &l_id_length);
    const int l_order = int(luaL_checkinteger(L, 2));
    luaL_checktype(L, 3, LUA_TBOOLEAN);
    const int l_always_active = lua_toboolean(L, 3);
    luaL_checktype(L, 4, LUA_TFUNCTION);

    LuaPluginState *l_plugin = pluginOf(L);
    LuaFnRef *l_ref = takeFnRef(L, 4);
    if (!l_plugin || !l_ref) {
        return luaL_error(L, "register_text_filter: no plugin is attached to this state");
    }

    const int l_registered = s_ffi->register_text_filter(
        l_id, l_id_length, l_order, l_always_active,
        luaFilterTrampoline, l_ref,
        l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()));
    if (!l_registered) {
        return luaL_error(L, "register_text_filter: the id is taken or invalid");
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int luaApiRegisterRuleAction(lua_State *L)
{
    size_t l_name_length = 0;
    const char *l_name = luaL_checklstring(L, 1, &l_name_length);
    const char *l_phase = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    const bool l_before = strcmp(l_phase, "before") == 0;
    if (!l_before && strcmp(l_phase, "after") != 0) {
        return luaL_error(L, "register_rule_action: the phase must be 'before' or 'after'");
    }

    LuaPluginState *l_plugin = pluginOf(L);
    LuaFnRef *l_ref = takeFnRef(L, 3);
    if (!l_plugin || !l_ref) {
        return luaL_error(L, "register_rule_action: no plugin is attached to this state");
    }

    const int l_registered = s_ffi->register_rule_action(
        l_name, l_name_length, l_before ? 1 : 0,
        luaRuleTrampoline, l_ref,
        l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()));
    if (!l_registered) {
        return luaL_error(L, "register_rule_action: the name is taken or invalid");
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int luaApiSubscribeEvent(lua_State *L)
{
    size_t l_name_length = 0;
    const char *l_name = luaL_checklstring(L, 1, &l_name_length);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    LuaPluginState *l_plugin = pluginOf(L);
    LuaFnRef *l_ref = takeFnRef(L, 2);
    if (!l_plugin || !l_ref) {
        return luaL_error(L, "subscribe_event: no plugin is attached to this state");
    }

    s_ffi->subscribe_event(l_name, l_name_length, luaEventTrampoline, l_ref,
                           l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()));
    return 0;
}

static int luaApiPublishEvent(lua_State *L)
{
    size_t l_name_length = 0;
    const char *l_name = luaL_checklstring(L, 1, &l_name_length);
    luaL_checktype(L, 2, LUA_TTABLE);

    QList<QByteArray> l_keys, l_values;
    lua_pushnil(L);
    while (lua_next(L, 2) != 0) {
        if (lua_type(L, -2) == LUA_TSTRING) {
            l_keys.append(QByteArray(lua_tostring(L, -2)));
            // A copy keeps lua_next's value untouched by the string
            // conversion; luaL_tolstring pushes its result.
            lua_pushvalue(L, -1);
            l_values.append(QByteArray(luaL_tolstring(L, -1, nullptr)));
            // Drops the conversion, the copy and the value; the key stays
            // for lua_next.
            lua_pop(L, 3);
        }
        else {
            lua_pop(L, 1);
        }
    }

    std::vector<const char *> l_key_ptrs, l_value_ptrs;
    for (int i = 0; i < l_keys.size(); i++) {
        l_key_ptrs.push_back(l_keys[i].constData());
        l_value_ptrs.push_back(l_values[i].constData());
    }
    s_ffi->publish_event(l_name, l_name_length, int(l_key_ptrs.size()), l_key_ptrs.data(), l_value_ptrs.data());
    return 0;
}

static int luaApiRegisterPermission(lua_State *L)
{
    size_t l_id_length = 0, l_display_length = 0, l_category_length = 0;
    const char *l_id = luaL_checklstring(L, 1, &l_id_length);
    const char *l_display = luaL_checklstring(L, 2, &l_display_length);
    const char *l_category = luaL_checklstring(L, 3, &l_category_length);

    LuaPluginState *l_plugin = pluginOf(L);
    if (!l_plugin) {
        return luaL_error(L, "register_permission: no plugin is attached to this state");
    }
    lua_pushboolean(L, s_ffi->register_permission(l_id, l_id_length, l_display, l_display_length,
                                                  l_category, l_category_length,
                                                  l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size())));
    return 1;
}

// akashi.grant(permission, audience, key) / akashi.revoke(...) place and
// lift standing grants: audience "person" with an IPID or "role" with a
// role id. The owner is stamped from the plugin, never script-supplied.
static int luaApiGrant(lua_State *L)
{
    size_t l_permission_length = 0, l_audience_length = 0, l_key_length = 0;
    const char *l_permission = luaL_checklstring(L, 1, &l_permission_length);
    const char *l_audience = luaL_checklstring(L, 2, &l_audience_length);
    const char *l_key = luaL_checklstring(L, 3, &l_key_length);

    LuaPluginState *l_plugin = pluginOf(L);
    if (!l_plugin || s_ffi->abi_version < 11) {
        return luaL_error(L, "grant: unavailable");
    }
    lua_pushboolean(L, s_ffi->grant(l_permission, l_permission_length, l_audience, l_audience_length,
                                    l_key, l_key_length,
                                    l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size())));
    return 1;
}

static int luaApiRevoke(lua_State *L)
{
    size_t l_permission_length = 0, l_audience_length = 0, l_key_length = 0;
    const char *l_permission = luaL_checklstring(L, 1, &l_permission_length);
    const char *l_audience = luaL_checklstring(L, 2, &l_audience_length);
    const char *l_key = luaL_checklstring(L, 3, &l_key_length);

    LuaPluginState *l_plugin = pluginOf(L);
    if (!l_plugin || s_ffi->abi_version < 11) {
        return luaL_error(L, "revoke: unavailable");
    }
    lua_pushboolean(L, s_ffi->revoke(l_permission, l_permission_length, l_audience, l_audience_length,
                                     l_key, l_key_length,
                                     l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size())));
    return 1;
}

static int luaApiConfigGet(lua_State *L)
{
    size_t l_key_length = 0, l_fallback_length = 0;
    const char *l_key = luaL_checklstring(L, 1, &l_key_length);
    const char *l_fallback = luaL_optlstring(L, 2, "", &l_fallback_length);

    LuaPluginState *l_plugin = pluginOf(L);
    if (!l_plugin) {
        return luaL_error(L, "config_get: no plugin is attached to this state");
    }
    size_t l_value_length = 0;
    const char *l_value = s_ffi->config_get(l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()),
                                            l_key, l_key_length, l_fallback, l_fallback_length, &l_value_length);
    lua_pushlstring(L, l_value, l_value_length);
    return 1;
}

static int luaApiConfigSet(lua_State *L)
{
    size_t l_key_length = 0, l_value_length = 0;
    const char *l_key = luaL_checklstring(L, 1, &l_key_length);
    const char *l_value = luaL_checklstring(L, 2, &l_value_length);

    LuaPluginState *l_plugin = pluginOf(L);
    if (!l_plugin || s_ffi->abi_version < 6) {
        return luaL_error(L, "config_set: unavailable");
    }
    lua_pushboolean(L, s_ffi->config_set(l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()),
                                         l_key, l_key_length, l_value, l_value_length));
    return 1;
}

static int luaApiConfigDeclare(lua_State *L)
{
    size_t l_key_length = 0, l_type_length = 0, l_default_length = 0, l_desc_length = 0;
    const char *l_key = luaL_checklstring(L, 1, &l_key_length);
    const char *l_type = luaL_checklstring(L, 2, &l_type_length);
    const char *l_default = luaL_optlstring(L, 3, "", &l_default_length);
    const char *l_desc = luaL_optlstring(L, 4, "", &l_desc_length);
    LuaPluginState *l_plugin = pluginOf(L);
    if (!l_plugin || s_ffi->abi_version < 8) {
        return luaL_error(L, "config_declare: unavailable");
    }
    lua_pushboolean(L, s_ffi->config_declare(l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()),
                                             l_key, l_key_length, l_type, l_type_length,
                                             l_default, l_default_length, l_desc, l_desc_length));
    return 1;
}

static int luaApiFsRead(lua_State *L)
{
    size_t l_path_length = 0;
    const char *l_path = luaL_checklstring(L, 1, &l_path_length);
    LuaPluginState *l_plugin = pluginOf(L);
    if (!l_plugin || s_ffi->abi_version < 6) {
        return luaL_error(L, "fs_read: unavailable");
    }
    size_t l_out_length = 0;
    const char *l_data = s_ffi->fs_read(l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()),
                                        l_path, l_path_length, &l_out_length);
    lua_pushlstring(L, l_data, l_out_length);
    return 1;
}

static int luaApiFsWrite(lua_State *L)
{
    size_t l_path_length = 0, l_data_length = 0;
    const char *l_path = luaL_checklstring(L, 1, &l_path_length);
    const char *l_data = luaL_checklstring(L, 2, &l_data_length);
    LuaPluginState *l_plugin = pluginOf(L);
    if (!l_plugin || s_ffi->abi_version < 6) {
        return luaL_error(L, "fs_write: unavailable");
    }
    lua_pushboolean(L, s_ffi->fs_write(l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()),
                                       l_path, l_path_length, l_data, l_data_length));
    return 1;
}

static int luaApiFsExists(lua_State *L)
{
    size_t l_path_length = 0;
    const char *l_path = luaL_checklstring(L, 1, &l_path_length);
    LuaPluginState *l_plugin = pluginOf(L);
    if (!l_plugin || s_ffi->abi_version < 6) {
        return luaL_error(L, "fs_exists: unavailable");
    }
    lua_pushboolean(L, s_ffi->fs_exists(l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()),
                                        l_path, l_path_length));
    return 1;
}

// The plugin's owner id, or a Lua error - shared by the discord verbs.
static LuaPluginState *luaDiscordOwner(lua_State *L, const char *f_verb)
{
    LuaPluginState *l_plugin = pluginOf(L);
    if (!l_plugin || s_ffi->abi_version < 7) {
        luaL_error(L, "%s: unavailable", f_verb);
        return nullptr;
    }
    return l_plugin;
}

static int luaApiDiscordBegin(lua_State *L)
{
    LuaPluginState *l_plugin = luaDiscordOwner(L, "discord_begin");
    s_ffi->discord_begin(l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()));
    return 0;
}

static int luaApiDiscordSet(lua_State *L)
{
    size_t l_key_length = 0, l_value_length = 0;
    const char *l_key = luaL_checklstring(L, 1, &l_key_length);
    const char *l_value = luaL_checklstring(L, 2, &l_value_length);
    LuaPluginState *l_plugin = luaDiscordOwner(L, "discord_set");
    s_ffi->discord_set(l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()),
                       l_key, l_key_length, l_value, l_value_length);
    return 0;
}

static int luaApiDiscordEmbedBegin(lua_State *L)
{
    LuaPluginState *l_plugin = luaDiscordOwner(L, "discord_embed_begin");
    s_ffi->discord_embed_begin(l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()));
    return 0;
}

static int luaApiDiscordEmbedSet(lua_State *L)
{
    size_t l_key_length = 0, l_value_length = 0;
    const char *l_key = luaL_checklstring(L, 1, &l_key_length);
    const char *l_value = luaL_checklstring(L, 2, &l_value_length);
    LuaPluginState *l_plugin = luaDiscordOwner(L, "discord_embed_set");
    s_ffi->discord_embed_set(l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()),
                             l_key, l_key_length, l_value, l_value_length);
    return 0;
}

static int luaApiDiscordEmbedFooter(lua_State *L)
{
    size_t l_text_length = 0, l_icon_length = 0;
    const char *l_text = luaL_checklstring(L, 1, &l_text_length);
    const char *l_icon = luaL_optlstring(L, 2, "", &l_icon_length);
    LuaPluginState *l_plugin = luaDiscordOwner(L, "discord_embed_footer");
    s_ffi->discord_embed_footer(l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()),
                                l_text, l_text_length, l_icon, l_icon_length);
    return 0;
}

static int luaApiDiscordEmbedAuthor(lua_State *L)
{
    size_t l_name_length = 0, l_url_length = 0, l_icon_length = 0;
    const char *l_name = luaL_checklstring(L, 1, &l_name_length);
    const char *l_url = luaL_optlstring(L, 2, "", &l_url_length);
    const char *l_icon = luaL_optlstring(L, 3, "", &l_icon_length);
    LuaPluginState *l_plugin = luaDiscordOwner(L, "discord_embed_author");
    s_ffi->discord_embed_author(l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()),
                                l_name, l_name_length, l_url, l_url_length, l_icon, l_icon_length);
    return 0;
}

static int luaApiDiscordEmbedField(lua_State *L)
{
    size_t l_name_length = 0, l_value_length = 0;
    const char *l_name = luaL_checklstring(L, 1, &l_name_length);
    const char *l_value = luaL_checklstring(L, 2, &l_value_length);
    const int l_inline = lua_toboolean(L, 3);
    LuaPluginState *l_plugin = luaDiscordOwner(L, "discord_embed_field");
    s_ffi->discord_embed_field(l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()),
                               l_name, l_name_length, l_value, l_value_length, l_inline);
    return 0;
}

static int luaApiDiscordEmbedEnd(lua_State *L)
{
    LuaPluginState *l_plugin = luaDiscordOwner(L, "discord_embed_end");
    s_ffi->discord_embed_end(l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()));
    return 0;
}

static int luaApiDiscordPost(lua_State *L)
{
    size_t l_url_length = 0;
    const char *l_url = luaL_checklstring(L, 1, &l_url_length);
    LuaPluginState *l_plugin = luaDiscordOwner(L, "discord_post");
    lua_pushboolean(L, s_ffi->discord_post(l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()),
                                           l_url, l_url_length));
    return 1;
}

// Reads an optional Lua table at stack index f_index into parallel arrays
// of string pointers and lengths, kept alive by f_storage.
static void luaCollectParams(lua_State *L, int f_index, QList<QByteArray> &f_storage,
                             std::vector<const char *> &f_ptrs, std::vector<size_t> &f_lengths)
{
    if (lua_type(L, f_index) != LUA_TTABLE) {
        return;
    }
    const int l_count = int(lua_rawlen(L, f_index));
    for (int i = 1; i <= l_count; i++) {
        lua_rawgeti(L, f_index, i);
        size_t l_length = 0;
        const char *l_value = lua_tolstring(L, -1, &l_length);
        f_storage.append(QByteArray(l_value ? l_value : "", int(l_length)));
        lua_pop(L, 1);
    }
    for (const QByteArray &l_param : f_storage) {
        f_ptrs.push_back(l_param.constData());
        f_lengths.push_back(size_t(l_param.size()));
    }
}

static int luaApiSqlExec(lua_State *L)
{
    size_t l_sql_length = 0;
    const char *l_sql = luaL_checklstring(L, 1, &l_sql_length);
    LuaPluginState *l_plugin = pluginOf(L);
    if (!l_plugin || s_ffi->abi_version < 6) {
        return luaL_error(L, "sql_exec: unavailable");
    }
    QList<QByteArray> l_storage;
    std::vector<const char *> l_ptrs;
    std::vector<size_t> l_lengths;
    luaCollectParams(L, 2, l_storage, l_ptrs, l_lengths);
    const int l_affected = s_ffi->sql_exec(l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()),
                                           l_sql, l_sql_length, int(l_ptrs.size()),
                                           l_ptrs.empty() ? nullptr : l_ptrs.data(),
                                           l_lengths.empty() ? nullptr : l_lengths.data());
    lua_pushinteger(L, l_affected);
    return 1;
}

// Hands a query row to a Lua callback as a { column = value } table.
static void luaSqlRowTrampoline(void *f_userdata, int f_count,
                                const char *const *f_columns, const char *const *f_values)
{
    lua_State *L = static_cast<lua_State *>(f_userdata);
    lua_pushvalue(L, 3); // the callback, kept at a fixed stack slot
    lua_newtable(L);
    for (int i = 0; i < f_count; i++) {
        lua_pushstring(L, f_values[i]);
        lua_setfield(L, -2, f_columns[i]);
    }
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        s_ffi->log_info(lua_tostring(L, -1), strlen(lua_tostring(L, -1)));
        lua_pop(L, 1);
    }
}

static int luaApiSqlQuery(lua_State *L)
{
    size_t l_sql_length = 0;
    const char *l_sql = luaL_checklstring(L, 1, &l_sql_length);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    LuaPluginState *l_plugin = pluginOf(L);
    if (!l_plugin || s_ffi->abi_version < 6) {
        return luaL_error(L, "sql_query: unavailable");
    }
    QList<QByteArray> l_storage;
    std::vector<const char *> l_ptrs;
    std::vector<size_t> l_lengths;
    luaCollectParams(L, 2, l_storage, l_ptrs, l_lengths);
    const int l_rows = s_ffi->sql_query(l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()),
                                        l_sql, l_sql_length, int(l_ptrs.size()),
                                        l_ptrs.empty() ? nullptr : l_ptrs.data(),
                                        l_lengths.empty() ? nullptr : l_lengths.data(),
                                        luaSqlRowTrampoline, L);
    lua_pushinteger(L, l_rows);
    return 1;
}

// Runs the migration body: the Lua function kept at stack slot 2.
static int luaMigrationTrampoline(void *f_userdata)
{
    lua_State *L = static_cast<lua_State *>(f_userdata);
    lua_pushvalue(L, 2);
    if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
        s_ffi->log_info(lua_tostring(L, -1), strlen(lua_tostring(L, -1)));
        lua_pop(L, 1);
        return 0;
    }
    const int l_ok = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return l_ok;
}

static int luaApiSqlMigrate(lua_State *L)
{
    const int l_version = int(luaL_checkinteger(L, 1));
    luaL_checktype(L, 2, LUA_TFUNCTION);
    LuaPluginState *l_plugin = pluginOf(L);
    if (!l_plugin || s_ffi->abi_version < 8) {
        return luaL_error(L, "sql_migrate: unavailable");
    }
    lua_pushboolean(L, s_ffi->sql_migrate(l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()),
                                          l_version, luaMigrationTrampoline, L));
    return 1;
}

static int luaApiSqlRead(lua_State *L)
{
    size_t l_source_length = 0, l_sql_length = 0;
    const char *l_source = luaL_checklstring(L, 1, &l_source_length);
    const char *l_sql = luaL_checklstring(L, 2, &l_sql_length);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    if (s_ffi->abi_version < 8) {
        return luaL_error(L, "sql_read: unavailable");
    }
    QList<QByteArray> l_storage;
    std::vector<const char *> l_ptrs;
    std::vector<size_t> l_lengths;
    luaCollectParams(L, 4, l_storage, l_ptrs, l_lengths);
    // The row callback sits at slot 3, where luaSqlRowTrampoline reads it.
    const int l_rows = s_ffi->sql_read(l_source, l_source_length, l_sql, l_sql_length, int(l_ptrs.size()),
                                       l_ptrs.empty() ? nullptr : l_ptrs.data(),
                                       l_lengths.empty() ? nullptr : l_lengths.data(),
                                       luaSqlRowTrampoline, L);
    lua_pushinteger(L, l_rows);
    return 1;
}

static int luaApiScheduleRepeating(lua_State *L)
{
    size_t l_job_length = 0, l_day_length = 0, l_time_length = 0;
    const char *l_job = luaL_checklstring(L, 1, &l_job_length);
    const char *l_day = luaL_checklstring(L, 2, &l_day_length);
    const char *l_time = luaL_checklstring(L, 3, &l_time_length);
    luaL_checktype(L, 4, LUA_TFUNCTION);
    LuaPluginState *l_plugin = pluginOf(L);
    LuaFnRef *l_ref = takeFnRef(L, 4);
    if (!l_plugin || !l_ref || s_ffi->abi_version < 9) {
        return luaL_error(L, "schedule_repeating: unavailable");
    }
    lua_pushboolean(L, s_ffi->schedule_repeating(l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()),
                                                 l_job, l_job_length, l_day, l_day_length, l_time, l_time_length,
                                                 luaConsoleTrampoline, l_ref));
    return 1;
}

static int luaApiScheduleOnce(lua_State *L)
{
    size_t l_job_length = 0, l_when_length = 0;
    const char *l_job = luaL_checklstring(L, 1, &l_job_length);
    const char *l_when = luaL_checklstring(L, 2, &l_when_length);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    LuaPluginState *l_plugin = pluginOf(L);
    LuaFnRef *l_ref = takeFnRef(L, 3);
    if (!l_plugin || !l_ref || s_ffi->abi_version < 9) {
        return luaL_error(L, "schedule_once: unavailable");
    }
    lua_pushboolean(L, s_ffi->schedule_once(l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()),
                                            l_job, l_job_length, l_when, l_when_length,
                                            luaConsoleTrampoline, l_ref));
    return 1;
}

static int luaApiScheduleCancel(lua_State *L)
{
    size_t l_job_length = 0;
    const char *l_job = luaL_checklstring(L, 1, &l_job_length);
    LuaPluginState *l_plugin = pluginOf(L);
    if (!l_plugin || s_ffi->abi_version < 9) {
        return luaL_error(L, "schedule_cancel: unavailable");
    }
    s_ffi->schedule_cancel(l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()), l_job, l_job_length);
    return 0;
}

static int luaApiScheduleNextRun(lua_State *L)
{
    size_t l_job_length = 0;
    const char *l_job = luaL_checklstring(L, 1, &l_job_length);
    LuaPluginState *l_plugin = pluginOf(L);
    if (!l_plugin || s_ffi->abi_version < 9) {
        return luaL_error(L, "schedule_next_run: unavailable");
    }
    size_t l_out_length = 0;
    const char *l_next = s_ffi->schedule_next_run(l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()),
                                                  l_job, l_job_length, &l_out_length);
    lua_pushlstring(L, l_next, l_out_length);
    return 1;
}

static int luaApiAreaGet(lua_State *L)
{
    const int l_area_id = int(luaL_checkinteger(L, 1));
    size_t l_key_length = 0;
    const char *l_key = luaL_checklstring(L, 2, &l_key_length);
    if (s_ffi->abi_version < 10) {
        return luaL_error(L, "area_get: unavailable");
    }
    size_t l_out_length = 0;
    const char *l_value = s_ffi->area_get(l_area_id, l_key, l_key_length, &l_out_length);
    lua_pushlstring(L, l_value, l_out_length);
    return 1;
}

static int luaApiAreaSet(lua_State *L)
{
    const int l_area_id = int(luaL_checkinteger(L, 1));
    size_t l_key_length = 0, l_value_length = 0;
    const char *l_key = luaL_checklstring(L, 2, &l_key_length);
    const char *l_value = luaL_checklstring(L, 3, &l_value_length);
    if (s_ffi->abi_version < 10) {
        return luaL_error(L, "area_set: unavailable");
    }
    lua_pushboolean(L, s_ffi->area_set(l_area_id, l_key, l_key_length, l_value, l_value_length));
    return 1;
}

static int luaApiFloorGet(lua_State *L)
{
    const int l_floor_id = int(luaL_checkinteger(L, 1));
    size_t l_key_length = 0;
    const char *l_key = luaL_checklstring(L, 2, &l_key_length);
    if (s_ffi->abi_version < 10) {
        return luaL_error(L, "floor_get: unavailable");
    }
    size_t l_out_length = 0;
    const char *l_value = s_ffi->floor_get(l_floor_id, l_key, l_key_length, &l_out_length);
    lua_pushlstring(L, l_value, l_out_length);
    return 1;
}

static int luaApiWorldAreaCount(lua_State *L)
{
    lua_pushinteger(L, s_ffi->abi_version < 10 ? 0 : s_ffi->world_area_count());
    return 1;
}

static int luaApiWorldFloorCount(lua_State *L)
{
    lua_pushinteger(L, s_ffi->abi_version < 10 ? 0 : s_ffi->world_floor_count());
    return 1;
}

// Hands an outbound packet to a Lua interceptor as (header, fields-table).
// The return decides its fate: false drops it, an array table replaces the
// fields, anything else lets it pass unchanged.
static int luaInterceptorTrampoline(void *f_userdata,
                                    const char *f_header, size_t f_header_length,
                                    int f_field_count, const char *const *f_fields, const size_t *f_field_lengths,
                                    AkashiPacketResult *f_result)
{
    LuaFnRef *l_ref = static_cast<LuaFnRef *>(f_userdata);
    lua_State *L = l_ref->state;
    lua_rawgeti(L, LUA_REGISTRYINDEX, l_ref->function_ref);
    lua_pushlstring(L, f_header, f_header_length);
    lua_createtable(L, f_field_count, 0);
    for (int i = 0; i < f_field_count; i++) {
        lua_pushlstring(L, f_fields[i], f_field_lengths[i]);
        lua_rawseti(L, -2, i + 1);
    }
    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        qCWarning(akashiScripting).noquote() << "lua-host: interceptor error:" << lua_tostring(L, -1);
        lua_pop(L, 1);
        return 1; // an erroring interceptor lets the packet through unchanged
    }

    int l_verdict = 1;
    if (lua_isboolean(L, -1) && !lua_toboolean(L, -1)) {
        l_verdict = 0; // false -> drop
    }
    else if (lua_istable(L, -1)) {
        // An array of fields replaces the packet's fields; header stays.
        const int l_count = int(lua_rawlen(L, -1));
        QList<QByteArray> l_storage;
        QVarLengthArray<const char *> l_ptrs(l_count);
        QVarLengthArray<size_t> l_lens(l_count);
        for (int i = 0; i < l_count; i++) {
            lua_rawgeti(L, -1, i + 1);
            size_t l_len = 0;
            const char *l_str = lua_tolstring(L, -1, &l_len);
            l_storage.append(QByteArray(l_str ? l_str : "", int(l_len)));
            lua_pop(L, 1);
        }
        for (int i = 0; i < l_count; i++) {
            l_ptrs[i] = l_storage[i].constData();
            l_lens[i] = size_t(l_storage[i].size());
        }
        s_ffi->packet_result_set(f_result, f_header, f_header_length, l_count, l_ptrs.data(), l_lens.data());
    }
    lua_pop(L, 1);
    return l_verdict;
}

static int luaApiRegisterOutboundInterceptor(lua_State *L)
{
    size_t l_header_length = 0;
    const char *l_header = luaL_optlstring(L, 1, "", &l_header_length);
    const int l_order = int(luaL_checkinteger(L, 2));
    luaL_checktype(L, 3, LUA_TFUNCTION);
    LuaPluginState *l_plugin = pluginOf(L);
    LuaFnRef *l_ref = takeFnRef(L, 3);
    if (!l_plugin || !l_ref || s_ffi->abi_version < 10) {
        return luaL_error(L, "register_outbound_interceptor: unavailable");
    }
    lua_pushboolean(L, s_ffi->register_outbound_interceptor(l_header, l_header_length, l_order,
                                                            luaInterceptorTrampoline, l_ref,
                                                            l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size())));
    return 1;
}

static AkashiCommandContext *checkContext(lua_State *L)
{
    luaL_argcheck(L, lua_islightuserdata(L, 1), 1, "command context expected");
    return static_cast<AkashiCommandContext *>(lua_touserdata(L, 1));
}

static int luaApiReply(lua_State *L)
{
    AkashiCommandContext *l_context = checkContext(L);
    size_t l_length = 0;
    const char *l_text = luaL_checklstring(L, 2, &l_length);
    s_ffi->reply(l_context, l_text, l_length);
    return 0;
}

static int luaApiReplyToArea(lua_State *L)
{
    AkashiCommandContext *l_context = checkContext(L);
    size_t l_length = 0;
    const char *l_text = luaL_checklstring(L, 2, &l_length);
    s_ffi->reply_to_area(l_context, l_text, l_length);
    return 0;
}

static int luaApiClientId(lua_State *L)
{
    lua_pushinteger(L, s_ffi->client_id(checkContext(L)));
    return 1;
}

static int luaApiAreaId(lua_State *L)
{
    lua_pushinteger(L, s_ffi->context_area_id(checkContext(L)));
    return 1;
}

static int luaApiPlayerName(lua_State *L)
{
    size_t l_length = 0;
    const char *l_value = s_ffi->context_player_name(checkContext(L), &l_length);
    lua_pushlstring(L, l_value, l_length);
    return 1;
}

static int luaApiCharacter(lua_State *L)
{
    size_t l_length = 0;
    const char *l_value = s_ffi->context_character(checkContext(L), &l_length);
    lua_pushlstring(L, l_value, l_length);
    return 1;
}

static int luaApiAreaName(lua_State *L)
{
    size_t l_length = 0;
    const char *l_value = s_ffi->context_area_name(checkContext(L), &l_length);
    lua_pushlstring(L, l_value, l_length);
    return 1;
}

static int luaApiIsAuthenticated(lua_State *L)
{
    lua_pushboolean(L, s_ffi->context_is_authenticated(checkContext(L)));
    return 1;
}

static int luaApiCanPerform(lua_State *L)
{
    AkashiCommandContext *l_context = checkContext(L);
    size_t l_length = 0;
    const char *l_permission = luaL_checklstring(L, 2, &l_length);
    lua_pushboolean(L, s_ffi->context_can_perform(l_context, l_permission, l_length));
    return 1;
}

// Target verbs take the 1-based command argument position, matching Lua's
// argument tables; the FFI counts from 0.
static int luaApiTargetId(lua_State *L)
{
    AkashiCommandContext *l_context = checkContext(L);
    const int l_index = int(luaL_checkinteger(L, 2)) - 1;
    lua_pushinteger(L, s_ffi->target_client_id(l_context, l_index));
    return 1;
}

static int luaApiTargetReply(lua_State *L)
{
    AkashiCommandContext *l_context = checkContext(L);
    const int l_index = int(luaL_checkinteger(L, 2)) - 1;
    size_t l_length = 0;
    const char *l_text = luaL_checklstring(L, 3, &l_length);
    lua_pushboolean(L, s_ffi->target_reply(l_context, l_index, l_text, l_length));
    return 1;
}

static int luaApiTargetHasSanction(lua_State *L)
{
    AkashiCommandContext *l_context = checkContext(L);
    const int l_index = int(luaL_checkinteger(L, 2)) - 1;
    size_t l_length = 0;
    const char *l_id = luaL_checklstring(L, 3, &l_length);
    lua_pushboolean(L, s_ffi->target_has_sanction(l_context, l_index, l_id, l_length));
    return 1;
}

static int luaApiTargetSetSanction(lua_State *L)
{
    AkashiCommandContext *l_context = checkContext(L);
    const int l_index = int(luaL_checkinteger(L, 2)) - 1;
    size_t l_length = 0;
    const char *l_id = luaL_checklstring(L, 3, &l_length);
    luaL_checktype(L, 4, LUA_TBOOLEAN);
    lua_pushboolean(L, s_ffi->target_set_sanction(l_context, l_index, l_id, l_length, lua_toboolean(L, 4)));
    return 1;
}

static int luaApiTargetChangeArea(lua_State *L)
{
    AkashiCommandContext *l_context = checkContext(L);
    const int l_index = int(luaL_checkinteger(L, 2)) - 1;
    const int l_area_id = int(luaL_checkinteger(L, 3));
    lua_pushboolean(L, s_ffi->target_change_area(l_context, l_index, l_area_id));
    return 1;
}

static const luaL_Reg s_akashi_api[] = {
    {"log", luaApiLog},
    {"console_print", luaApiConsolePrint},
    {"register_command", luaApiRegisterCommand},
    {"register_text_filter", luaApiRegisterTextFilter},
    {"register_permission", luaApiRegisterPermission},
    {"grant", luaApiGrant},
    {"revoke", luaApiRevoke},
    {"register_rule_action", luaApiRegisterRuleAction},
    {"register_console_action", luaApiRegisterConsoleAction},
    {"subscribe_event", luaApiSubscribeEvent},
    {"publish_event", luaApiPublishEvent},
    {"config_get", luaApiConfigGet},
    {"config_set", luaApiConfigSet},
    {"config_declare", luaApiConfigDeclare},
    {"fs_read", luaApiFsRead},
    {"fs_write", luaApiFsWrite},
    {"fs_exists", luaApiFsExists},
    {"sql_exec", luaApiSqlExec},
    {"sql_query", luaApiSqlQuery},
    {"sql_migrate", luaApiSqlMigrate},
    {"sql_read", luaApiSqlRead},
    {"schedule_repeating", luaApiScheduleRepeating},
    {"schedule_once", luaApiScheduleOnce},
    {"schedule_cancel", luaApiScheduleCancel},
    {"schedule_next_run", luaApiScheduleNextRun},
    {"area_get", luaApiAreaGet},
    {"area_set", luaApiAreaSet},
    {"floor_get", luaApiFloorGet},
    {"world_area_count", luaApiWorldAreaCount},
    {"world_floor_count", luaApiWorldFloorCount},
    {"register_outbound_interceptor", luaApiRegisterOutboundInterceptor},
    {"discord_begin", luaApiDiscordBegin},
    {"discord_set", luaApiDiscordSet},
    {"discord_embed_begin", luaApiDiscordEmbedBegin},
    {"discord_embed_set", luaApiDiscordEmbedSet},
    {"discord_embed_footer", luaApiDiscordEmbedFooter},
    {"discord_embed_author", luaApiDiscordEmbedAuthor},
    {"discord_embed_field", luaApiDiscordEmbedField},
    {"discord_embed_end", luaApiDiscordEmbedEnd},
    {"discord_post", luaApiDiscordPost},
    {"reply", luaApiReply},
    {"reply_to_area", luaApiReplyToArea},
    {"client_id", luaApiClientId},
    {"area_id", luaApiAreaId},
    {"player_name", luaApiPlayerName},
    {"character", luaApiCharacter},
    {"area_name", luaApiAreaName},
    {"is_authenticated", luaApiIsAuthenticated},
    {"can_perform", luaApiCanPerform},
    {"target_id", luaApiTargetId},
    {"target_reply", luaApiTargetReply},
    {"target_has_sanction", luaApiTargetHasSanction},
    {"target_set_sanction", luaApiTargetSetSanction},
    {"target_change_area", luaApiTargetChangeArea},
    {nullptr, nullptr},
};

// The akashi.script-host.lua service: one interpreter per Lua plugin.
class LuaScriptHost : public akashi::IScriptPluginHost
{
  public:
    QString serviceId() const override { return QStringLiteral("akashi.script-host.lua"); }
    akashi::ServiceVersion serviceVersion() const override { return {1, 1, 0}; }
    QString runtime() const override { return QStringLiteral("lua"); }

    // A Lua plugin is one .lua file whose declaration header carries the
    // manifest; finding them is this host's business, not the manager's.
    // Scripts live in the host's own subfolder, created on first run -
    // only files in there are this host's to load.
    QList<akashi::PluginInfo> discoverScriptPlugins(const QString &f_plugin_dir) override
    {
        QList<akashi::PluginInfo> l_manifests;
        const QString l_script_dir = f_plugin_dir + QStringLiteral("/lua");
        QDir().mkpath(l_script_dir);
        const QDir l_dir(l_script_dir);
        const QStringList l_files = l_dir.entryList({QStringLiteral("*.lua")}, QDir::Files, QDir::Name);
        for (const QString &l_file : l_files) {
            const auto l_info = akashi::PluginManager::parseScriptHeader(l_dir.absoluteFilePath(l_file));
            if (l_info && l_info->runtime == runtime()) {
                l_manifests.append(*l_info);
            }
        }
        return l_manifests;
    }

    bool loadScriptPlugin(const QString &f_plugin_id, const QString &f_entry_path) override
    {
        if (s_plugins.contains(f_plugin_id)) {
            return false;
        }

        auto l_plugin = new LuaPluginState;
        l_plugin->owner_id = f_plugin_id.toUtf8();
        l_plugin->state = luaL_newstate();

        lua_State *L = l_plugin->state;
        luaL_openlibs(L);
        lua_pushlightuserdata(L, l_plugin);
        lua_setfield(L, LUA_REGISTRYINDEX, "akashi.plugin");
        luaL_newlib(L, s_akashi_api);
        lua_setglobal(L, "akashi");

        if (luaL_dofile(L, QFile::encodeName(f_entry_path).constData()) != LUA_OK) {
            qCWarning(akashiScripting).noquote() << "lua-host: error in" << f_entry_path << ":" << lua_tostring(L, -1);
            // Half-done registrations must not survive the failed load.
            s_ffi->unregister_owner(l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()));
            releasePlugin(l_plugin);
            return false;
        }
        s_plugins.insert(f_plugin_id, l_plugin);
        return true;
    }

    void unloadScriptPlugin(const QString &f_plugin_id) override
    {
        LuaPluginState *l_plugin = s_plugins.take(f_plugin_id);
        if (!l_plugin) {
            return;
        }
        // The plugin's registrations go before its state, so no handler can
        // fire into a closed interpreter.
        s_ffi->unregister_owner(l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()));
        releasePlugin(l_plugin);
    }

    void unloadAll()
    {
        const QStringList l_ids = s_plugins.keys();
        for (const QString &l_id : l_ids) {
            unloadScriptPlugin(l_id);
        }
    }

  private:
    static void releasePlugin(LuaPluginState *f_plugin)
    {
        for (LuaFnRef *l_ref : std::as_const(f_plugin->fn_refs)) {
            delete l_ref;
        }
        lua_close(f_plugin->state);
        delete f_plugin;
    }
};

QString LuaHostPlugin::id() const { return QStringLiteral("akashi.lua-host"); }
akashi::ServiceVersion LuaHostPlugin::pluginVersion() const { return {1, 2, 0}; }

bool LuaHostPlugin::load(akashi::ServiceRegistry &services)
{
    auto l_ffi_service = services.resolve<ScriptingFfiService>(QStringLiteral("akashi.scripting-ffi"));
    if (!l_ffi_service) {
        qCWarning(akashiScripting) << "lua-host: the scripting FFI is not available";
        return false;
    }
    s_ffi = l_ffi_service->table();

    m_host = std::make_shared<LuaScriptHost>();
    if (!services.registerService(m_host, id())) {
        qCWarning(akashiScripting) << "lua-host: service id already taken";
        s_ffi = nullptr;
        return false;
    }

    // The manifest cannot know the vendored interpreter's version, so the
    // credit line is composed here and overrides it.
    if (auto l_plugins = services.resolve<akashi::PluginManager>(QStringLiteral("akashi.plugins"))) {
        l_plugins->registerAbout(id(), QStringLiteral("Runs the server's .lua plugins. Embeds " LUA_RELEASE " by PUC-Rio (MIT license)."));
    }

    qCInfo(akashiScripting).noquote() << "lua-host: providing" << m_host->serviceId() << "with" << LUA_RELEASE;
    return true;
}

void LuaHostPlugin::shutdown(akashi::ServiceRegistry &services)
{
    // Normally the manager unloads the dependent Lua plugins first; this
    // catches anything left.
    if (m_host) {
        m_host->unloadAll();
    }
    services.unregisterService(QStringLiteral("akashi.script-host.lua"));
    m_host.reset();
    s_ffi = nullptr;
}
