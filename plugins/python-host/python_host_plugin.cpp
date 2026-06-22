// Python.h must come first, and a debug build must not demand the debug
// python library that plain installations do not ship.
#ifdef _DEBUG
#undef _DEBUG
#include <Python.h>
#define _DEBUG
#else
#include <Python.h>
#endif

#include "python_host_plugin.h"

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

// Everything one Python plugin owns: its module namespace and its handlers.
struct PythonPluginState
{
    QByteArray owner_id;
    PyObject *globals = nullptr;
    QList<PyObject *> handlers;
};

static const AkashiFfi *s_ffi = nullptr;
static QHash<QString, PythonPluginState *> s_plugins;

// The plugin whose entry script is running; registrations attribute to it.
static PythonPluginState *s_loading_plugin = nullptr;

// Runs a registered Python handler as handler(context, [args]).
static void pyCommandTrampoline(void *f_userdata, AkashiCommandContext *f_context,
                                int f_argc, const char *const *f_argv)
{
    PyObject *l_handler = static_cast<PyObject *>(f_userdata);
    PyObject *l_context = PyCapsule_New(f_context, "akashi.context", nullptr);
    PyObject *l_args = PyList_New(f_argc);
    for (int i = 0; i < f_argc; i++) {
        PyList_SetItem(l_args, i, PyUnicode_FromString(f_argv[i]));
    }

    PyObject *l_result = PyObject_CallFunctionObjArgs(l_handler, l_context, l_args, nullptr);
    if (!l_result) {
        PyErr_Print();
    }
    Py_XDECREF(l_result);
    Py_DECREF(l_args);
    Py_DECREF(l_context);
}

static int pyStringArg(PyObject *f_object, const char **f_text, Py_ssize_t *f_length)
{
    *f_text = PyUnicode_AsUTF8AndSize(f_object, f_length);
    return *f_text ? 1 : 0;
}

static PyObject *pyApiLog(PyObject *, PyObject *f_args)
{
    PyObject *l_text_obj = nullptr;
    if (!PyArg_ParseTuple(f_args, "U", &l_text_obj)) {
        return nullptr;
    }
    const char *l_text = nullptr;
    Py_ssize_t l_length = 0;
    if (!pyStringArg(l_text_obj, &l_text, &l_length)) {
        return nullptr;
    }
    s_ffi->log_info(l_text, size_t(l_length));
    Py_RETURN_NONE;
}

static PyObject *pyApiRegisterCommand(PyObject *, PyObject *f_args)
{
    PyObject *l_name_obj = nullptr, *l_usage_obj = nullptr, *l_description_obj = nullptr, *l_handler = nullptr;
    if (!PyArg_ParseTuple(f_args, "UUUO", &l_name_obj, &l_usage_obj, &l_description_obj, &l_handler)) {
        return nullptr;
    }
    if (!PyCallable_Check(l_handler)) {
        PyErr_SetString(PyExc_TypeError, "handler must be callable");
        return nullptr;
    }
    if (!s_loading_plugin) {
        PyErr_SetString(PyExc_RuntimeError, "register_command is only available while the plugin loads");
        return nullptr;
    }

    const char *l_name = nullptr, *l_usage = nullptr, *l_description = nullptr;
    Py_ssize_t l_name_length = 0, l_usage_length = 0, l_description_length = 0;
    if (!pyStringArg(l_name_obj, &l_name, &l_name_length) ||
        !pyStringArg(l_usage_obj, &l_usage, &l_usage_length) ||
        !pyStringArg(l_description_obj, &l_description, &l_description_length)) {
        return nullptr;
    }

    Py_INCREF(l_handler);
    const int l_registered = s_ffi->register_command(
        l_name, size_t(l_name_length), l_usage, size_t(l_usage_length),
        l_description, size_t(l_description_length),
        "", 0, 0,
        pyCommandTrampoline, l_handler,
        s_loading_plugin->owner_id.constData(), size_t(s_loading_plugin->owner_id.size()));
    if (!l_registered) {
        Py_DECREF(l_handler);
        PyErr_SetString(PyExc_ValueError, "register_command: the name is taken or invalid");
        return nullptr;
    }
    s_loading_plugin->handlers.append(l_handler);
    Py_RETURN_NONE;
}

static AkashiCommandContext *pyContextArg(PyObject *f_capsule)
{
    return static_cast<AkashiCommandContext *>(PyCapsule_GetPointer(f_capsule, "akashi.context"));
}

static PyObject *pyApiReply(PyObject *, PyObject *f_args)
{
    PyObject *l_capsule = nullptr, *l_text_obj = nullptr;
    if (!PyArg_ParseTuple(f_args, "OU", &l_capsule, &l_text_obj)) {
        return nullptr;
    }
    AkashiCommandContext *l_context = pyContextArg(l_capsule);
    if (!l_context) {
        return nullptr;
    }
    const char *l_text = nullptr;
    Py_ssize_t l_length = 0;
    if (!pyStringArg(l_text_obj, &l_text, &l_length)) {
        return nullptr;
    }
    s_ffi->reply(l_context, l_text, size_t(l_length));
    Py_RETURN_NONE;
}

static PyObject *pyApiReplyToArea(PyObject *, PyObject *f_args)
{
    PyObject *l_capsule = nullptr, *l_text_obj = nullptr;
    if (!PyArg_ParseTuple(f_args, "OU", &l_capsule, &l_text_obj)) {
        return nullptr;
    }
    AkashiCommandContext *l_context = pyContextArg(l_capsule);
    if (!l_context) {
        return nullptr;
    }
    const char *l_text = nullptr;
    Py_ssize_t l_length = 0;
    if (!pyStringArg(l_text_obj, &l_text, &l_length)) {
        return nullptr;
    }
    s_ffi->reply_to_area(l_context, l_text, size_t(l_length));
    Py_RETURN_NONE;
}

static PyObject *pyApiClientId(PyObject *, PyObject *f_args)
{
    PyObject *l_capsule = nullptr;
    if (!PyArg_ParseTuple(f_args, "O", &l_capsule)) {
        return nullptr;
    }
    AkashiCommandContext *l_context = pyContextArg(l_capsule);
    if (!l_context) {
        return nullptr;
    }
    return PyLong_FromLong(s_ffi->client_id(l_context));
}

static PyMethodDef s_akashi_methods[] = {
    {"log", pyApiLog, METH_VARARGS, "Writes one line to the server log."},
    {"register_command", pyApiRegisterCommand, METH_VARARGS, "Registers a chat command: register_command(name, usage, description, handler)."},
    {"reply", pyApiReply, METH_VARARGS, "Replies to the invoker of the running command."},
    {"reply_to_area", pyApiReplyToArea, METH_VARARGS, "Replies to everyone in the invoker's area."},
    {"client_id", pyApiClientId, METH_VARARGS, "The invoker's client id."},
    {nullptr, nullptr, 0, nullptr},
};

static PyModuleDef s_akashi_module = {
    PyModuleDef_HEAD_INIT, "akashi", "The akashi scripting surface.", -1,
    s_akashi_methods, nullptr, nullptr, nullptr, nullptr};

static PyObject *pyInitAkashiModule()
{
    return PyModule_Create(&s_akashi_module);
}

// The akashi.script-host.python service: one namespace per Python plugin
// inside one embedded interpreter.
class PythonScriptHost : public akashi::IScriptPluginHost
{
  public:
    QString serviceId() const override { return QStringLiteral("akashi.script-host.python"); }
    akashi::ServiceVersion serviceVersion() const override { return {1, 0, 0}; }
    QString runtime() const override { return QStringLiteral("python"); }

    bool loadScriptPlugin(const QString &f_plugin_id, const QString &f_entry_path) override
    {
        if (s_plugins.contains(f_plugin_id)) {
            return false;
        }
        QFile l_file(f_entry_path);
        if (!l_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "python-host: unable to open" << f_entry_path;
            return false;
        }
        const QByteArray l_source = l_file.readAll();

        auto l_plugin = new PythonPluginState;
        l_plugin->owner_id = f_plugin_id.toUtf8();
        l_plugin->globals = PyDict_New();
        PyDict_SetItemString(l_plugin->globals, "__builtins__", PyEval_GetBuiltins());
        PyObject *l_name = PyUnicode_FromString(l_plugin->owner_id.constData());
        PyDict_SetItemString(l_plugin->globals, "__name__", l_name);
        Py_DECREF(l_name);

        s_loading_plugin = l_plugin;
        PyObject *l_result = PyRun_String(l_source.constData(), Py_file_input,
                                          l_plugin->globals, l_plugin->globals);
        s_loading_plugin = nullptr;

        if (!l_result) {
            PyErr_Print();
            qWarning() << "python-host: error in" << f_entry_path;
            releasePlugin(l_plugin);
            return false;
        }
        Py_DECREF(l_result);
        s_plugins.insert(f_plugin_id, l_plugin);
        return true;
    }

    void unloadScriptPlugin(const QString &f_plugin_id) override
    {
        PythonPluginState *l_plugin = s_plugins.take(f_plugin_id);
        if (!l_plugin) {
            return;
        }
        // The plugin's commands go before its handlers, so no handler can
        // fire into released objects.
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
    static void releasePlugin(PythonPluginState *f_plugin)
    {
        for (PyObject *l_handler : std::as_const(f_plugin->handlers)) {
            Py_DECREF(l_handler);
        }
        Py_XDECREF(f_plugin->globals);
        delete f_plugin;
    }
};

QString PythonHostPlugin::id() const { return QStringLiteral("akashi.python-host"); }
akashi::ServiceVersion PythonHostPlugin::pluginVersion() const { return {1, 0, 0}; }

bool PythonHostPlugin::load(akashi::ServiceRegistry &services)
{
    auto l_ffi_service = services.resolve<ScriptingFfiService>(QStringLiteral("akashi.scripting-ffi"));
    if (!l_ffi_service) {
        qWarning() << "python-host: the scripting FFI is not available";
        return false;
    }
    s_ffi = l_ffi_service->table();

    if (!Py_IsInitialized()) {
        PyImport_AppendInittab("akashi", pyInitAkashiModule);
        Py_Initialize();
    }

    m_host = std::make_shared<PythonScriptHost>();
    if (!services.registerService(m_host, id())) {
        qWarning() << "python-host: service id already taken";
        s_ffi = nullptr;
        return false;
    }
    qInfo().noquote() << "python-host: providing" << m_host->serviceId() << "with Python" << QString::fromUtf8(Py_GetVersion()).section(' ', 0, 0);
    return true;
}

void PythonHostPlugin::shutdown(akashi::ServiceRegistry &services)
{
    // Normally the manager unloads the dependent Python plugins first; this
    // catches anything left.
    if (m_host) {
        m_host->unloadAll();
    }
    services.unregisterService(QStringLiteral("akashi.script-host.python"));
    m_host.reset();
    if (Py_IsInitialized()) {
        Py_FinalizeEx();
    }
    s_ffi = nullptr;
}
