#include "lua_host_plugin.h"

#include "akashi_ffi.h"
#include "scripting_ffi_service.h"

#include "akashi/script_plugin_host.h"
#include "akashi/service_registry.h"

#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QHash>
#include <QList>

#include <cstring>

extern "C" {
#include "lua/lauxlib.h"
#include "lua/lua.h"
#include "lua/lualib.h"
}

// One registered Lua command handler: the state it lives in and a registry
// reference to the function. Owned by the host, released with its plugin.
struct LuaCommandRef
{
    lua_State *state = nullptr;
    int function_ref = LUA_NOREF;
};

// Everything one Lua plugin owns: its interpreter and its handlers.
struct LuaPluginState
{
    lua_State *state = nullptr;
    QByteArray owner_id;
    QList<LuaCommandRef *> command_refs;
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

// Runs a registered Lua handler as (context, args) with args as an array.
static void luaCommandTrampoline(void *f_userdata, AkashiCommandContext *f_context,
                                 int f_argc, const char *const *f_argv)
{
    LuaCommandRef *l_ref = static_cast<LuaCommandRef *>(f_userdata);
    lua_State *L = l_ref->state;
    lua_rawgeti(L, LUA_REGISTRYINDEX, l_ref->function_ref);
    lua_pushlightuserdata(L, f_context);
    lua_createtable(L, f_argc, 0);
    for (int i = 0; i < f_argc; i++) {
        lua_pushstring(L, f_argv[i]);
        lua_rawseti(L, -2, i + 1);
    }
    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        qWarning().noquote() << "lua-host: handler error:" << lua_tostring(L, -1);
        lua_pop(L, 1);
    }
}

static int luaApiLog(lua_State *L)
{
    size_t l_length = 0;
    const char *l_text = luaL_checklstring(L, 1, &l_length);
    s_ffi->log_info(l_text, l_length);
    return 0;
}

static int luaApiRegisterCommand(lua_State *L)
{
    size_t l_name_length = 0, l_usage_length = 0, l_description_length = 0;
    const char *l_name = luaL_checklstring(L, 1, &l_name_length);
    const char *l_usage = luaL_checklstring(L, 2, &l_usage_length);
    const char *l_description = luaL_checklstring(L, 3, &l_description_length);
    luaL_checktype(L, 4, LUA_TFUNCTION);

    LuaPluginState *l_plugin = pluginOf(L);
    if (!l_plugin) {
        return luaL_error(L, "register_command: no plugin is attached to this state");
    }

    auto l_ref = new LuaCommandRef;
    l_ref->state = L;
    lua_pushvalue(L, 4);
    l_ref->function_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    const int l_registered = s_ffi->register_command(
        l_name, l_name_length, l_usage, l_usage_length,
        l_description, l_description_length,
        "", 0, 0,
        luaCommandTrampoline, l_ref,
        l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()));
    if (!l_registered) {
        luaL_unref(L, LUA_REGISTRYINDEX, l_ref->function_ref);
        delete l_ref;
        return luaL_error(L, "register_command: the name is taken or invalid");
    }
    l_plugin->command_refs.append(l_ref);
    lua_pushboolean(L, 1);
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

static const luaL_Reg s_akashi_api[] = {
    {"log", luaApiLog},
    {"register_command", luaApiRegisterCommand},
    {"reply", luaApiReply},
    {"reply_to_area", luaApiReplyToArea},
    {"client_id", luaApiClientId},
    {nullptr, nullptr},
};

// The akashi.script-host.lua service: one interpreter per Lua plugin.
class LuaScriptHost : public akashi::IScriptPluginHost
{
  public:
    QString serviceId() const override { return QStringLiteral("akashi.script-host.lua"); }
    akashi::ServiceVersion serviceVersion() const override { return {1, 0, 0}; }
    QString runtime() const override { return QStringLiteral("lua"); }

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
            qWarning().noquote() << "lua-host: error in" << f_entry_path << ":" << lua_tostring(L, -1);
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
        // The plugin's commands go before its state, so no handler can fire
        // into a closed interpreter.
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
        for (LuaCommandRef *l_ref : std::as_const(f_plugin->command_refs)) {
            delete l_ref;
        }
        lua_close(f_plugin->state);
        delete f_plugin;
    }
};

QString LuaHostPlugin::id() const { return QStringLiteral("akashi.lua-host"); }
akashi::ServiceVersion LuaHostPlugin::pluginVersion() const { return {1, 0, 0}; }

bool LuaHostPlugin::load(akashi::ServiceRegistry &services)
{
    auto l_ffi_service = services.resolve<ScriptingFfiService>(QStringLiteral("akashi.scripting-ffi"));
    if (!l_ffi_service) {
        qWarning() << "lua-host: the scripting FFI is not available";
        return false;
    }
    s_ffi = l_ffi_service->table();

    m_host = std::make_shared<LuaScriptHost>();
    if (!services.registerService(m_host, id())) {
        qWarning() << "lua-host: service id already taken";
        s_ffi = nullptr;
        return false;
    }
    qInfo().noquote() << "lua-host: providing" << m_host->serviceId() << "with" << LUA_RELEASE;
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
