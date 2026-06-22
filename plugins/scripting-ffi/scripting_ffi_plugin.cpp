// AI-generated: written by Claude.
#include "scripting_ffi_plugin.h"

#include "scripting_ffi_service.h"

#include "akashi/service_registry.h"
#include "core/command_context.h"
#include "core/command_registry.h"
#include "core/command_spec.h"

#include <QByteArray>
#include <QDebug>
#include <QList>

#include <vector>

// The C functions need reachable state; the plugin fills these on load and
// clears them on shutdown, after which every call turns into a no-op.
static akashi::CommandRegistry *s_commands = nullptr;

static QString toString(const char *f_text, size_t f_length)
{
    return f_text ? QString::fromUtf8(f_text, int(f_length)) : QString();
}

static void ffiLogInfo(const char *f_text, size_t f_text_length)
{
    qInfo().noquote() << "[scripting]" << toString(f_text, f_text_length);
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

    const bool l_registered = s_commands->registerCommand(l_spec, [f_handler, f_userdata](akashi::CommandContext &f_context) {
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
                  int(l_argv.size()), l_argv.data());
    }, toString(f_owner_id, f_owner_id_length));
    return l_registered ? 1 : 0;
}

static void ffiUnregisterOwner(const char *f_owner_id, size_t f_owner_id_length)
{
    if (s_commands) {
        s_commands->unregisterAll(toString(f_owner_id, f_owner_id_length));
    }
}

static void ffiReply(AkashiCommandContext *f_context, const char *f_text, size_t f_text_length)
{
    if (f_context) {
        reinterpret_cast<akashi::CommandContext *>(f_context)->reply(toString(f_text, f_text_length));
    }
}

static void ffiReplyToArea(AkashiCommandContext *f_context, const char *f_text, size_t f_text_length)
{
    if (f_context) {
        reinterpret_cast<akashi::CommandContext *>(f_context)->replyToArea(toString(f_text, f_text_length));
    }
}

static int ffiClientId(AkashiCommandContext *f_context)
{
    return f_context ? reinterpret_cast<akashi::CommandContext *>(f_context)->clientId() : -1;
}

static const AkashiFfi s_table = {
    AKASHI_FFI_ABI_VERSION,
    ffiLogInfo,
    ffiRegisterCommand,
    ffiUnregisterOwner,
    ffiReply,
    ffiReplyToArea,
    ffiClientId,
};

namespace {

class ServiceImpl : public ScriptingFfiService
{
  public:
    const AkashiFfi *table() const override { return &s_table; }
};

} // namespace

QString ScriptingFfiPlugin::id() const { return QStringLiteral("akashi.scripting-ffi"); }
akashi::ServiceVersion ScriptingFfiPlugin::pluginVersion() const { return {1, 0, 0}; }

bool ScriptingFfiPlugin::load(akashi::ServiceRegistry &services)
{
    auto l_commands = services.resolve<akashi::CommandRegistry>(QStringLiteral("akashi.commands"));
    if (!l_commands) {
        qWarning() << "scripting-ffi: command registry not available";
        return false;
    }
    s_commands = l_commands.get();

    m_service = std::make_shared<ServiceImpl>();
    if (!services.registerService(m_service, id())) {
        qWarning() << "scripting-ffi: service id already taken";
        s_commands = nullptr;
        return false;
    }

    qInfo() << "scripting-ffi: C surface available, ABI version" << AKASHI_FFI_ABI_VERSION;
    return true;
}

void ScriptingFfiPlugin::shutdown(akashi::ServiceRegistry &services)
{
    services.unregisterService(QStringLiteral("akashi.scripting-ffi"));
    s_commands = nullptr;
    m_service.reset();
}
