// The stable ABI keeps one built plugin working across Python 3.x versions.
#define Py_LIMITED_API 0x030A0000

// Python.h must come first, and a debug build must not demand the debug
// python library that plain installations do not ship.
#ifdef _DEBUG
#undef _DEBUG
#include <Python.h>
#define _DEBUG
#else
#include <Python.h>
#endif

#include "akashi/logging_categories.h"
#include "akashi/script_plugin_host.h"
#include "akashi/service_registry.h"
#include "akashi_ffi.h"
#include "core/plugin_manager.h"
#include "python_host_plugin.h"
#include "scripting_ffi_service.h"

#include <QByteArray>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QList>

#include <cstring>
#include <vector>

struct PythonPluginState;

// One registered Python callable and the plugin it belongs to.
struct PyFnRef
{
    PyObject *callable = nullptr;
    PythonPluginState *plugin = nullptr;
};

// Everything one Python plugin owns: its module namespace and its handlers.
struct PythonPluginState
{
    QByteArray owner_id;
    PyObject *globals = nullptr;
    QList<PyFnRef *> fn_refs;
};

static const AkashiFfi *s_ffi = nullptr;
static QHash<QString, PythonPluginState *> s_plugins;

// The plugin whose code is running - its entry script or one of its
// handlers - so registrations and config reads attribute to it.
static PythonPluginState *s_active_plugin = nullptr;

// Runs a registered Python handler as handler(context, [args]).
static void pyCommandTrampoline(void *f_userdata, AkashiCommandContext *f_context,
                                int f_argc, const char *const *f_argv)
{
    PyFnRef *l_ref = static_cast<PyFnRef *>(f_userdata);
    PyObject *l_context = PyCapsule_New(f_context, "akashi.context", nullptr);
    PyObject *l_args = PyList_New(f_argc);
    for (int i = 0; i < f_argc; i++) {
        PyList_SetItem(l_args, i, PyUnicode_FromString(f_argv[i]));
    }

    PythonPluginState *l_previous = s_active_plugin;
    s_active_plugin = l_ref->plugin;
    PyObject *l_result = PyObject_CallFunctionObjArgs(l_ref->callable, l_context, l_args, nullptr);
    s_active_plugin = l_previous;

    if (!l_result) {
        PyErr_Print();
    }
    Py_XDECREF(l_result);
    Py_DECREF(l_args);
    Py_DECREF(l_context);
}

// Runs a Python text filter: a returned str rewrites the message, False
// drops it, anything else leaves it unchanged.
static int pyFilterTrampoline(void *f_userdata, const char *f_text, size_t f_text_length,
                              AkashiTextResult *f_result)
{
    PyFnRef *l_ref = static_cast<PyFnRef *>(f_userdata);
    PyObject *l_text = PyUnicode_FromStringAndSize(f_text, Py_ssize_t(f_text_length));

    PythonPluginState *l_previous = s_active_plugin;
    s_active_plugin = l_ref->plugin;
    PyObject *l_result = PyObject_CallFunctionObjArgs(l_ref->callable, l_text, nullptr);
    s_active_plugin = l_previous;
    Py_DECREF(l_text);

    if (!l_result) {
        PyErr_Print();
        return 1;
    }
    int l_keep = 1;
    if (PyUnicode_Check(l_result)) {
        Py_ssize_t l_length = 0;
        const char *l_rewritten = PyUnicode_AsUTF8AndSize(l_result, &l_length);
        if (l_rewritten) {
            s_ffi->text_result_set(f_result, l_rewritten, size_t(l_length));
        }
    }
    else if (l_result == Py_False) {
        l_keep = 0;
    }
    Py_DECREF(l_result);
    return l_keep;
}

// Runs a Python rule action with one info dict: ids, the event payload and
// the attached arguments. A returned str blocks with that reason; False
// blocks with the stock reason; anything else allows.
static void pyRuleTrampoline(void *f_userdata,
                             int f_player_id, int f_area_id, int f_floor_id,
                             int f_payload_count, const char *const *f_payload_keys, const char *const *f_payload_values,
                             int f_argument_count, const char *const *f_argument_keys, const char *const *f_argument_values,
                             AkashiRuleResult *f_result)
{
    PyFnRef *l_ref = static_cast<PyFnRef *>(f_userdata);
    PyObject *l_info = PyDict_New();
    PyObject *l_number = PyLong_FromLong(f_player_id);
    PyDict_SetItemString(l_info, "player_id", l_number);
    Py_DECREF(l_number);
    l_number = PyLong_FromLong(f_area_id);
    PyDict_SetItemString(l_info, "area_id", l_number);
    Py_DECREF(l_number);
    l_number = PyLong_FromLong(f_floor_id);
    PyDict_SetItemString(l_info, "floor_id", l_number);
    Py_DECREF(l_number);

    PyObject *l_payload = PyDict_New();
    for (int i = 0; i < f_payload_count; i++) {
        PyObject *l_value = PyUnicode_FromString(f_payload_values[i]);
        PyDict_SetItemString(l_payload, f_payload_keys[i], l_value);
        Py_DECREF(l_value);
    }
    PyDict_SetItemString(l_info, "payload", l_payload);
    Py_DECREF(l_payload);

    PyObject *l_arguments = PyDict_New();
    for (int i = 0; i < f_argument_count; i++) {
        PyObject *l_value = PyUnicode_FromString(f_argument_values[i]);
        PyDict_SetItemString(l_arguments, f_argument_keys[i], l_value);
        Py_DECREF(l_value);
    }
    PyDict_SetItemString(l_info, "args", l_arguments);
    Py_DECREF(l_arguments);

    PythonPluginState *l_previous = s_active_plugin;
    s_active_plugin = l_ref->plugin;
    PyObject *l_result = PyObject_CallFunctionObjArgs(l_ref->callable, l_info, nullptr);
    s_active_plugin = l_previous;
    Py_DECREF(l_info);

    if (!l_result) {
        PyErr_Print();
        return;
    }
    if (f_result) {
        if (PyUnicode_Check(l_result)) {
            Py_ssize_t l_length = 0;
            const char *l_reason = PyUnicode_AsUTF8AndSize(l_result, &l_length);
            if (l_reason) {
                s_ffi->rule_result_block(f_result, l_reason, size_t(l_length));
            }
        }
        else if (l_result == Py_False) {
            s_ffi->rule_result_block(f_result, "", 0);
        }
    }
    Py_DECREF(l_result);
}

// Runs a Python event handler with the payload as a dict.
static void pyEventTrampoline(void *f_userdata, int f_count,
                              const char *const *f_keys, const char *const *f_values)
{
    PyFnRef *l_ref = static_cast<PyFnRef *>(f_userdata);
    PyObject *l_payload = PyDict_New();
    for (int i = 0; i < f_count; i++) {
        PyObject *l_value = PyUnicode_FromString(f_values[i]);
        PyDict_SetItemString(l_payload, f_keys[i], l_value);
        Py_DECREF(l_value);
    }

    PythonPluginState *l_previous = s_active_plugin;
    s_active_plugin = l_ref->plugin;
    PyObject *l_result = PyObject_CallFunctionObjArgs(l_ref->callable, l_payload, nullptr);
    s_active_plugin = l_previous;

    if (!l_result) {
        PyErr_Print();
    }
    Py_XDECREF(l_result);
    Py_DECREF(l_payload);
}

static int pyStringArg(PyObject *f_object, const char **f_text, Py_ssize_t *f_length)
{
    *f_text = PyUnicode_AsUTF8AndSize(f_object, f_length);
    return *f_text ? 1 : 0;
}

static PyFnRef *takeFnRef(PyObject *f_callable)
{
    if (!s_active_plugin) {
        return nullptr;
    }
    Py_INCREF(f_callable);
    auto l_ref = new PyFnRef;
    l_ref->callable = f_callable;
    l_ref->plugin = s_active_plugin;
    s_active_plugin->fn_refs.append(l_ref);
    return l_ref;
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

static PyObject *pyApiConsolePrint(PyObject *, PyObject *f_args)
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
    // An older core's table ends before console_print; fall back to the log.
    if (s_ffi->abi_version >= 5) {
        s_ffi->console_print(l_text, size_t(l_length));
    }
    else {
        s_ffi->log_info(l_text, size_t(l_length));
    }
    Py_RETURN_NONE;
}

static PyObject *pyApiRegisterCommand(PyObject *, PyObject *f_args)
{
    PyObject *l_name_obj = nullptr, *l_usage_obj = nullptr, *l_description_obj = nullptr, *l_handler = nullptr;
    PyObject *l_permission_obj = nullptr;
    int l_min_args = 0;
    if (!PyArg_ParseTuple(f_args, "UUUO|Ui", &l_name_obj, &l_usage_obj, &l_description_obj, &l_handler,
                          &l_permission_obj, &l_min_args)) {
        return nullptr;
    }
    if (!PyCallable_Check(l_handler)) {
        PyErr_SetString(PyExc_TypeError, "handler must be callable");
        return nullptr;
    }

    const char *l_name = nullptr, *l_usage = nullptr, *l_description = nullptr, *l_permission = "";
    Py_ssize_t l_name_length = 0, l_usage_length = 0, l_description_length = 0, l_permission_length = 0;
    if (!pyStringArg(l_name_obj, &l_name, &l_name_length) ||
        !pyStringArg(l_usage_obj, &l_usage, &l_usage_length) ||
        !pyStringArg(l_description_obj, &l_description, &l_description_length)) {
        return nullptr;
    }
    if (l_permission_obj && !pyStringArg(l_permission_obj, &l_permission, &l_permission_length)) {
        return nullptr;
    }

    PyFnRef *l_ref = takeFnRef(l_handler);
    if (!l_ref) {
        PyErr_SetString(PyExc_RuntimeError, "no plugin is active");
        return nullptr;
    }

    const int l_registered = s_ffi->register_command(
        l_name, size_t(l_name_length), l_usage, size_t(l_usage_length),
        l_description, size_t(l_description_length),
        l_permission, size_t(l_permission_length), l_min_args,
        pyCommandTrampoline, l_ref,
        l_ref->plugin->owner_id.constData(), size_t(l_ref->plugin->owner_id.size()));
    if (!l_registered) {
        PyErr_SetString(PyExc_ValueError, "register_command: the name is taken or invalid");
        return nullptr;
    }
    Py_RETURN_NONE;
}

static PyObject *pyApiRegisterTextFilter(PyObject *, PyObject *f_args)
{
    PyObject *l_id_obj = nullptr, *l_handler = nullptr;
    int l_order = 0, l_always_active = 0;
    if (!PyArg_ParseTuple(f_args, "UipO", &l_id_obj, &l_order, &l_always_active, &l_handler)) {
        return nullptr;
    }
    if (!PyCallable_Check(l_handler)) {
        PyErr_SetString(PyExc_TypeError, "handler must be callable");
        return nullptr;
    }
    const char *l_id = nullptr;
    Py_ssize_t l_id_length = 0;
    if (!pyStringArg(l_id_obj, &l_id, &l_id_length)) {
        return nullptr;
    }

    PyFnRef *l_ref = takeFnRef(l_handler);
    if (!l_ref) {
        PyErr_SetString(PyExc_RuntimeError, "no plugin is active");
        return nullptr;
    }

    const int l_registered = s_ffi->register_text_filter(
        l_id, size_t(l_id_length), l_order, l_always_active,
        pyFilterTrampoline, l_ref,
        l_ref->plugin->owner_id.constData(), size_t(l_ref->plugin->owner_id.size()));
    if (!l_registered) {
        PyErr_SetString(PyExc_ValueError, "register_text_filter: the id is taken or invalid");
        return nullptr;
    }
    Py_RETURN_NONE;
}

static PyObject *pyApiRegisterRuleAction(PyObject *, PyObject *f_args)
{
    PyObject *l_name_obj = nullptr, *l_phase_obj = nullptr, *l_handler = nullptr;
    if (!PyArg_ParseTuple(f_args, "UUO", &l_name_obj, &l_phase_obj, &l_handler)) {
        return nullptr;
    }
    if (!PyCallable_Check(l_handler)) {
        PyErr_SetString(PyExc_TypeError, "handler must be callable");
        return nullptr;
    }
    const char *l_name = nullptr, *l_phase = nullptr;
    Py_ssize_t l_name_length = 0, l_phase_length = 0;
    if (!pyStringArg(l_name_obj, &l_name, &l_name_length) ||
        !pyStringArg(l_phase_obj, &l_phase, &l_phase_length)) {
        return nullptr;
    }
    const bool l_before = strcmp(l_phase, "before") == 0;
    if (!l_before && strcmp(l_phase, "after") != 0) {
        PyErr_SetString(PyExc_ValueError, "the phase must be 'before' or 'after'");
        return nullptr;
    }

    PyFnRef *l_ref = takeFnRef(l_handler);
    if (!l_ref) {
        PyErr_SetString(PyExc_RuntimeError, "no plugin is active");
        return nullptr;
    }

    const int l_registered = s_ffi->register_rule_action(
        l_name, size_t(l_name_length), l_before ? 1 : 0,
        pyRuleTrampoline, l_ref,
        l_ref->plugin->owner_id.constData(), size_t(l_ref->plugin->owner_id.size()));
    if (!l_registered) {
        PyErr_SetString(PyExc_ValueError, "register_rule_action: the name is taken or invalid");
        return nullptr;
    }
    Py_RETURN_NONE;
}

// Runs a registered Python console task.
static void pyConsoleTrampoline(void *f_userdata)
{
    PyFnRef *l_ref = static_cast<PyFnRef *>(f_userdata);
    PythonPluginState *l_previous = s_active_plugin;
    s_active_plugin = l_ref->plugin;
    PyObject *l_result = PyObject_CallFunctionObjArgs(l_ref->callable, nullptr);
    s_active_plugin = l_previous;
    if (!l_result) {
        PyErr_Print();
    }
    Py_XDECREF(l_result);
}

static PyObject *pyApiRegisterConsoleAction(PyObject *, PyObject *f_args)
{
    PyObject *l_title_obj = nullptr, *l_handler = nullptr;
    if (!PyArg_ParseTuple(f_args, "UO", &l_title_obj, &l_handler)) {
        return nullptr;
    }
    if (!PyCallable_Check(l_handler)) {
        PyErr_SetString(PyExc_TypeError, "handler must be callable");
        return nullptr;
    }
    const char *l_title = nullptr;
    Py_ssize_t l_title_length = 0;
    if (!pyStringArg(l_title_obj, &l_title, &l_title_length)) {
        return nullptr;
    }

    PyFnRef *l_ref = takeFnRef(l_handler);
    if (!l_ref) {
        PyErr_SetString(PyExc_RuntimeError, "no plugin is active");
        return nullptr;
    }
    return PyBool_FromLong(s_ffi->register_console_action(l_title, size_t(l_title_length), pyConsoleTrampoline, l_ref,
                                                          l_ref->plugin->owner_id.constData(), size_t(l_ref->plugin->owner_id.size())));
}

static PyObject *pyApiSubscribeEvent(PyObject *, PyObject *f_args)
{
    PyObject *l_name_obj = nullptr, *l_handler = nullptr;
    if (!PyArg_ParseTuple(f_args, "UO", &l_name_obj, &l_handler)) {
        return nullptr;
    }
    if (!PyCallable_Check(l_handler)) {
        PyErr_SetString(PyExc_TypeError, "handler must be callable");
        return nullptr;
    }
    const char *l_name = nullptr;
    Py_ssize_t l_name_length = 0;
    if (!pyStringArg(l_name_obj, &l_name, &l_name_length)) {
        return nullptr;
    }

    PyFnRef *l_ref = takeFnRef(l_handler);
    if (!l_ref) {
        PyErr_SetString(PyExc_RuntimeError, "no plugin is active");
        return nullptr;
    }
    s_ffi->subscribe_event(l_name, size_t(l_name_length), pyEventTrampoline, l_ref,
                           l_ref->plugin->owner_id.constData(), size_t(l_ref->plugin->owner_id.size()));
    Py_RETURN_NONE;
}

static PyObject *pyApiPublishEvent(PyObject *, PyObject *f_args)
{
    PyObject *l_name_obj = nullptr, *l_payload = nullptr;
    if (!PyArg_ParseTuple(f_args, "UO!", &l_name_obj, &PyDict_Type, &l_payload)) {
        return nullptr;
    }
    const char *l_name = nullptr;
    Py_ssize_t l_name_length = 0;
    if (!pyStringArg(l_name_obj, &l_name, &l_name_length)) {
        return nullptr;
    }

    QList<QByteArray> l_keys, l_values;
    PyObject *l_key = nullptr, *l_value = nullptr;
    Py_ssize_t l_position = 0;
    while (PyDict_Next(l_payload, &l_position, &l_key, &l_value)) {
        if (!PyUnicode_Check(l_key)) {
            continue;
        }
        PyObject *l_value_str = PyObject_Str(l_value);
        if (!l_value_str) {
            PyErr_Clear();
            continue;
        }
        Py_ssize_t l_key_length = 0, l_value_length = 0;
        const char *l_key_text = PyUnicode_AsUTF8AndSize(l_key, &l_key_length);
        const char *l_value_text = PyUnicode_AsUTF8AndSize(l_value_str, &l_value_length);
        if (l_key_text && l_value_text) {
            l_keys.append(QByteArray(l_key_text, int(l_key_length)));
            l_values.append(QByteArray(l_value_text, int(l_value_length)));
        }
        Py_DECREF(l_value_str);
    }

    std::vector<const char *> l_key_ptrs, l_value_ptrs;
    for (int i = 0; i < l_keys.size(); i++) {
        l_key_ptrs.push_back(l_keys[i].constData());
        l_value_ptrs.push_back(l_values[i].constData());
    }
    s_ffi->publish_event(l_name, size_t(l_name_length), int(l_key_ptrs.size()), l_key_ptrs.data(), l_value_ptrs.data());
    Py_RETURN_NONE;
}

static PyObject *pyApiRegisterPermission(PyObject *, PyObject *f_args)
{
    PyObject *l_id_obj = nullptr, *l_display_obj = nullptr, *l_category_obj = nullptr;
    if (!PyArg_ParseTuple(f_args, "UUU", &l_id_obj, &l_display_obj, &l_category_obj)) {
        return nullptr;
    }
    const char *l_id = nullptr, *l_display = nullptr, *l_category = nullptr;
    Py_ssize_t l_id_length = 0, l_display_length = 0, l_category_length = 0;
    if (!pyStringArg(l_id_obj, &l_id, &l_id_length) ||
        !pyStringArg(l_display_obj, &l_display, &l_display_length) ||
        !pyStringArg(l_category_obj, &l_category, &l_category_length)) {
        return nullptr;
    }
    if (!s_active_plugin) {
        PyErr_SetString(PyExc_RuntimeError, "no plugin is active");
        return nullptr;
    }
    s_ffi->register_permission(l_id, size_t(l_id_length), l_display, size_t(l_display_length),
                               l_category, size_t(l_category_length),
                               s_active_plugin->owner_id.constData(), size_t(s_active_plugin->owner_id.size()));
    Py_RETURN_NONE;
}

static PyObject *pyApiConfigGet(PyObject *, PyObject *f_args)
{
    PyObject *l_key_obj = nullptr, *l_fallback_obj = nullptr;
    if (!PyArg_ParseTuple(f_args, "U|U", &l_key_obj, &l_fallback_obj)) {
        return nullptr;
    }
    const char *l_key = nullptr, *l_fallback = "";
    Py_ssize_t l_key_length = 0, l_fallback_length = 0;
    if (!pyStringArg(l_key_obj, &l_key, &l_key_length)) {
        return nullptr;
    }
    if (l_fallback_obj && !pyStringArg(l_fallback_obj, &l_fallback, &l_fallback_length)) {
        return nullptr;
    }
    if (!s_active_plugin) {
        PyErr_SetString(PyExc_RuntimeError, "no plugin is active");
        return nullptr;
    }
    size_t l_value_length = 0;
    const char *l_value = s_ffi->config_get(s_active_plugin->owner_id.constData(), size_t(s_active_plugin->owner_id.size()),
                                            l_key, size_t(l_key_length), l_fallback, size_t(l_fallback_length), &l_value_length);
    return PyUnicode_FromStringAndSize(l_value, Py_ssize_t(l_value_length));
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

static PyObject *pyApiAreaId(PyObject *, PyObject *f_args)
{
    PyObject *l_capsule = nullptr;
    if (!PyArg_ParseTuple(f_args, "O", &l_capsule)) {
        return nullptr;
    }
    AkashiCommandContext *l_context = pyContextArg(l_capsule);
    if (!l_context) {
        return nullptr;
    }
    return PyLong_FromLong(s_ffi->context_area_id(l_context));
}

static PyObject *pyStringGetter(PyObject *f_args, const char *(*f_getter)(AkashiCommandContext *, size_t *))
{
    PyObject *l_capsule = nullptr;
    if (!PyArg_ParseTuple(f_args, "O", &l_capsule)) {
        return nullptr;
    }
    AkashiCommandContext *l_context = pyContextArg(l_capsule);
    if (!l_context) {
        return nullptr;
    }
    size_t l_length = 0;
    const char *l_value = f_getter(l_context, &l_length);
    return PyUnicode_FromStringAndSize(l_value, Py_ssize_t(l_length));
}

static PyObject *pyApiPlayerName(PyObject *, PyObject *f_args)
{
    return pyStringGetter(f_args, s_ffi->context_player_name);
}

static PyObject *pyApiCharacter(PyObject *, PyObject *f_args)
{
    return pyStringGetter(f_args, s_ffi->context_character);
}

static PyObject *pyApiAreaName(PyObject *, PyObject *f_args)
{
    return pyStringGetter(f_args, s_ffi->context_area_name);
}

static PyObject *pyApiIsAuthenticated(PyObject *, PyObject *f_args)
{
    PyObject *l_capsule = nullptr;
    if (!PyArg_ParseTuple(f_args, "O", &l_capsule)) {
        return nullptr;
    }
    AkashiCommandContext *l_context = pyContextArg(l_capsule);
    if (!l_context) {
        return nullptr;
    }
    return PyBool_FromLong(s_ffi->context_is_authenticated(l_context));
}

static PyObject *pyApiCanPerform(PyObject *, PyObject *f_args)
{
    PyObject *l_capsule = nullptr, *l_permission_obj = nullptr;
    if (!PyArg_ParseTuple(f_args, "OU", &l_capsule, &l_permission_obj)) {
        return nullptr;
    }
    AkashiCommandContext *l_context = pyContextArg(l_capsule);
    if (!l_context) {
        return nullptr;
    }
    const char *l_permission = nullptr;
    Py_ssize_t l_length = 0;
    if (!pyStringArg(l_permission_obj, &l_permission, &l_length)) {
        return nullptr;
    }
    return PyBool_FromLong(s_ffi->context_can_perform(l_context, l_permission, size_t(l_length)));
}

static PyObject *pyApiTargetId(PyObject *, PyObject *f_args)
{
    PyObject *l_capsule = nullptr;
    int l_index = 0;
    if (!PyArg_ParseTuple(f_args, "Oi", &l_capsule, &l_index)) {
        return nullptr;
    }
    AkashiCommandContext *l_context = pyContextArg(l_capsule);
    if (!l_context) {
        return nullptr;
    }
    return PyLong_FromLong(s_ffi->target_client_id(l_context, l_index));
}

static PyObject *pyApiTargetReply(PyObject *, PyObject *f_args)
{
    PyObject *l_capsule = nullptr, *l_text_obj = nullptr;
    int l_index = 0;
    if (!PyArg_ParseTuple(f_args, "OiU", &l_capsule, &l_index, &l_text_obj)) {
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
    return PyBool_FromLong(s_ffi->target_reply(l_context, l_index, l_text, size_t(l_length)));
}

static PyObject *pyApiTargetHasSanction(PyObject *, PyObject *f_args)
{
    PyObject *l_capsule = nullptr, *l_id_obj = nullptr;
    int l_index = 0;
    if (!PyArg_ParseTuple(f_args, "OiU", &l_capsule, &l_index, &l_id_obj)) {
        return nullptr;
    }
    AkashiCommandContext *l_context = pyContextArg(l_capsule);
    if (!l_context) {
        return nullptr;
    }
    const char *l_id = nullptr;
    Py_ssize_t l_length = 0;
    if (!pyStringArg(l_id_obj, &l_id, &l_length)) {
        return nullptr;
    }
    return PyBool_FromLong(s_ffi->target_has_sanction(l_context, l_index, l_id, size_t(l_length)));
}

static PyObject *pyApiTargetSetSanction(PyObject *, PyObject *f_args)
{
    PyObject *l_capsule = nullptr, *l_id_obj = nullptr;
    int l_index = 0, l_active = 0;
    if (!PyArg_ParseTuple(f_args, "OiUp", &l_capsule, &l_index, &l_id_obj, &l_active)) {
        return nullptr;
    }
    AkashiCommandContext *l_context = pyContextArg(l_capsule);
    if (!l_context) {
        return nullptr;
    }
    const char *l_id = nullptr;
    Py_ssize_t l_length = 0;
    if (!pyStringArg(l_id_obj, &l_id, &l_length)) {
        return nullptr;
    }
    return PyBool_FromLong(s_ffi->target_set_sanction(l_context, l_index, l_id, size_t(l_length), l_active));
}

static PyObject *pyApiTargetChangeArea(PyObject *, PyObject *f_args)
{
    PyObject *l_capsule = nullptr;
    int l_index = 0, l_area_id = 0;
    if (!PyArg_ParseTuple(f_args, "Oii", &l_capsule, &l_index, &l_area_id)) {
        return nullptr;
    }
    AkashiCommandContext *l_context = pyContextArg(l_capsule);
    if (!l_context) {
        return nullptr;
    }
    return PyBool_FromLong(s_ffi->target_change_area(l_context, l_index, l_area_id));
}

static PyMethodDef s_akashi_methods[] = {
    {"log", pyApiLog, METH_VARARGS, "Writes one line to the server log."},
    {"console_print", pyApiConsolePrint, METH_VARARGS, "Prints a line to the operator running the current console task."},
    {"register_command", pyApiRegisterCommand, METH_VARARGS, "register_command(name, usage, description, handler, permission='', min_args=0)."},
    {"register_text_filter", pyApiRegisterTextFilter, METH_VARARGS, "register_text_filter(id, order, always_active, handler); the handler returns a str rewrite, False to drop, or None."},
    {"register_permission", pyApiRegisterPermission, METH_VARARGS, "register_permission(id, display_name, category)."},
    {"register_rule_action", pyApiRegisterRuleAction, METH_VARARGS, "register_rule_action(name, phase, handler); a before handler may return a str or False to block."},
    {"register_console_action", pyApiRegisterConsoleAction, METH_VARARGS, "register_console_action(title, handler): puts a task on the server console's menu."},
    {"subscribe_event", pyApiSubscribeEvent, METH_VARARGS, "subscribe_event(name, handler); the handler receives the payload dict."},
    {"publish_event", pyApiPublishEvent, METH_VARARGS, "publish_event(name, payload_dict)."},
    {"config_get", pyApiConfigGet, METH_VARARGS, "config_get(key, fallback='') from the plugin's config file."},
    {"reply", pyApiReply, METH_VARARGS, "Replies to the invoker of the running command."},
    {"reply_to_area", pyApiReplyToArea, METH_VARARGS, "Replies to everyone in the invoker's area."},
    {"client_id", pyApiClientId, METH_VARARGS, "The invoker's client id."},
    {"area_id", pyApiAreaId, METH_VARARGS, "The invoker's area id."},
    {"player_name", pyApiPlayerName, METH_VARARGS, "The invoker's OOC name."},
    {"character", pyApiCharacter, METH_VARARGS, "The invoker's character."},
    {"area_name", pyApiAreaName, METH_VARARGS, "The invoker's area name."},
    {"is_authenticated", pyApiIsAuthenticated, METH_VARARGS, "Whether the invoker is a logged-in moderator."},
    {"can_perform", pyApiCanPerform, METH_VARARGS, "Whether the invoker holds a permission."},
    {"target_id", pyApiTargetId, METH_VARARGS, "target_id(ctx, argument_index): the client id the argument names, or -1."},
    {"target_reply", pyApiTargetReply, METH_VARARGS, "target_reply(ctx, argument_index, text)."},
    {"target_has_sanction", pyApiTargetHasSanction, METH_VARARGS, "target_has_sanction(ctx, argument_index, sanction_id)."},
    {"target_set_sanction", pyApiTargetSetSanction, METH_VARARGS, "target_set_sanction(ctx, argument_index, sanction_id, active)."},
    {"target_change_area", pyApiTargetChangeArea, METH_VARARGS, "target_change_area(ctx, argument_index, area_id)."},
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
    akashi::ServiceVersion serviceVersion() const override { return {1, 1, 0}; }
    QString runtime() const override { return QStringLiteral("python"); }

    // A Python plugin is one .py file whose declaration header carries the
    // manifest; finding them is this host's business, not the manager's.
    QList<akashi::PluginInfo> discoverScriptPlugins(const QString &f_plugin_dir) override
    {
        QList<akashi::PluginInfo> l_manifests;
        const QDir l_dir(f_plugin_dir);
        const QStringList l_files = l_dir.entryList({QStringLiteral("*.py")}, QDir::Files, QDir::Name);
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
        QFile l_file(f_entry_path);
        if (!l_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCWarning(akashiScripting) << "python-host: unable to open" << f_entry_path;
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

        // The stable ABI has no PyRun_String, so the entry compiles and runs
        // through the interpreter's own builtins.
        PyObject *l_builtins = PyEval_GetBuiltins();
        PyObject *l_compile = PyDict_GetItemString(l_builtins, "compile");
        PyObject *l_exec = PyDict_GetItemString(l_builtins, "exec");

        PythonPluginState *l_previous = s_active_plugin;
        s_active_plugin = l_plugin;
        PyObject *l_result = nullptr;
        PyObject *l_code = PyObject_CallFunction(l_compile, "sss", l_source.constData(),
                                                 QFile::encodeName(f_entry_path).constData(), "exec");
        if (l_code) {
            l_result = PyObject_CallFunction(l_exec, "OO", l_code, l_plugin->globals);
            Py_DECREF(l_code);
        }
        s_active_plugin = l_previous;

        if (!l_result) {
            PyErr_Print();
            qCWarning(akashiScripting) << "python-host: error in" << f_entry_path;
            // Half-done registrations must not survive the failed load.
            s_ffi->unregister_owner(l_plugin->owner_id.constData(), size_t(l_plugin->owner_id.size()));
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
        // The plugin's registrations go before its handlers, so no handler
        // can fire into released objects.
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
        for (PyFnRef *l_ref : std::as_const(f_plugin->fn_refs)) {
            Py_DECREF(l_ref->callable);
            delete l_ref;
        }
        Py_XDECREF(f_plugin->globals);
        delete f_plugin;
    }
};

QString PythonHostPlugin::id() const { return QStringLiteral("akashi.python-host"); }
akashi::ServiceVersion PythonHostPlugin::pluginVersion() const { return {1, 1, 0}; }

bool PythonHostPlugin::load(akashi::ServiceRegistry &services)
{
    auto l_ffi_service = services.resolve<ScriptingFfiService>(QStringLiteral("akashi.scripting-ffi"));
    if (!l_ffi_service) {
        qCWarning(akashiScripting) << "python-host: the scripting FFI is not available";
        return false;
    }
    s_ffi = l_ffi_service->table();

    if (!Py_IsInitialized()) {
        PyImport_AppendInittab("akashi", pyInitAkashiModule);
        Py_Initialize();
    }

    m_host = std::make_shared<PythonScriptHost>();
    if (!services.registerService(m_host, id())) {
        qCWarning(akashiScripting) << "python-host: service id already taken";
        s_ffi = nullptr;
        return false;
    }
    qCInfo(akashiScripting).noquote() << "python-host: providing" << m_host->serviceId() << "with Python" << QString::fromUtf8(Py_GetVersion()).section(' ', 0, 0);
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
