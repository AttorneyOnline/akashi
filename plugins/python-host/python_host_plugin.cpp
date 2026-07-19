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
                             int f_player_state_id, int f_area_id, int f_floor_id,
                             int f_payload_count, const char *const *f_payload_keys, const char *const *f_payload_values,
                             int f_argument_count, const char *const *f_argument_keys, const char *const *f_argument_values,
                             AkashiRuleResult *f_result)
{
    PyFnRef *l_ref = static_cast<PyFnRef *>(f_userdata);
    PyObject *l_info = PyDict_New();
    PyObject *l_number = PyLong_FromLong(f_player_state_id);
    PyDict_SetItemString(l_info, "player_state_id", l_number);
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

// akashi.grant(permission, audience, key) / akashi.revoke(...) place and
// lift standing grants: audience "person" with an IPID or "role" with a
// role id. The owner is stamped from the plugin, never script-supplied.
static PyObject *pyGrantOrRevoke(PyObject *f_args, bool f_grant)
{
    PyObject *l_permission_obj = nullptr, *l_audience_obj = nullptr, *l_key_obj = nullptr;
    if (!PyArg_ParseTuple(f_args, "UUU", &l_permission_obj, &l_audience_obj, &l_key_obj)) {
        return nullptr;
    }
    const char *l_permission = nullptr, *l_audience = nullptr, *l_key = nullptr;
    Py_ssize_t l_permission_length = 0, l_audience_length = 0, l_key_length = 0;
    if (!pyStringArg(l_permission_obj, &l_permission, &l_permission_length) ||
        !pyStringArg(l_audience_obj, &l_audience, &l_audience_length) ||
        !pyStringArg(l_key_obj, &l_key, &l_key_length)) {
        return nullptr;
    }
    if (!s_active_plugin || s_ffi->abi_version < 11) {
        PyErr_SetString(PyExc_RuntimeError, "grant/revoke unavailable");
        return nullptr;
    }
    const auto l_call = f_grant ? s_ffi->grant : s_ffi->revoke;
    return PyBool_FromLong(l_call(l_permission, size_t(l_permission_length),
                                  l_audience, size_t(l_audience_length),
                                  l_key, size_t(l_key_length),
                                  s_active_plugin->owner_id.constData(), size_t(s_active_plugin->owner_id.size())));
}

static PyObject *pyApiGrant(PyObject *, PyObject *f_args)
{
    return pyGrantOrRevoke(f_args, true);
}

static PyObject *pyApiRevoke(PyObject *, PyObject *f_args)
{
    return pyGrantOrRevoke(f_args, false);
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

static PyObject *pyApiConfigSet(PyObject *, PyObject *f_args)
{
    PyObject *l_key_obj = nullptr, *l_value_obj = nullptr;
    if (!PyArg_ParseTuple(f_args, "UU", &l_key_obj, &l_value_obj)) {
        return nullptr;
    }
    const char *l_key = nullptr, *l_value = nullptr;
    Py_ssize_t l_key_length = 0, l_value_length = 0;
    if (!pyStringArg(l_key_obj, &l_key, &l_key_length) || !pyStringArg(l_value_obj, &l_value, &l_value_length)) {
        return nullptr;
    }
    if (!s_active_plugin || s_ffi->abi_version < 6) {
        PyErr_SetString(PyExc_RuntimeError, "config_set unavailable");
        return nullptr;
    }
    const int l_ok = s_ffi->config_set(s_active_plugin->owner_id.constData(), size_t(s_active_plugin->owner_id.size()),
                                       l_key, size_t(l_key_length), l_value, size_t(l_value_length));
    return PyBool_FromLong(l_ok);
}

static PyObject *pyApiConfigDeclare(PyObject *, PyObject *f_args)
{
    const char *l_key = nullptr, *l_type = nullptr, *l_default = "", *l_desc = "";
    Py_ssize_t l_key_length = 0, l_type_length = 0, l_default_length = 0, l_desc_length = 0;
    if (!PyArg_ParseTuple(f_args, "s#s#|s#s#", &l_key, &l_key_length, &l_type, &l_type_length,
                          &l_default, &l_default_length, &l_desc, &l_desc_length)) {
        return nullptr;
    }
    if (!s_active_plugin || s_ffi->abi_version < 8) {
        PyErr_SetString(PyExc_RuntimeError, "config_declare unavailable");
        return nullptr;
    }
    const int l_ok = s_ffi->config_declare(s_active_plugin->owner_id.constData(), size_t(s_active_plugin->owner_id.size()),
                                           l_key, size_t(l_key_length), l_type, size_t(l_type_length),
                                           l_default, size_t(l_default_length), l_desc, size_t(l_desc_length));
    return PyBool_FromLong(l_ok);
}

static PyObject *pyApiFsRead(PyObject *, PyObject *f_args)
{
    PyObject *l_path_obj = nullptr;
    if (!PyArg_ParseTuple(f_args, "U", &l_path_obj)) {
        return nullptr;
    }
    const char *l_path = nullptr;
    Py_ssize_t l_path_length = 0;
    if (!pyStringArg(l_path_obj, &l_path, &l_path_length)) {
        return nullptr;
    }
    if (!s_active_plugin || s_ffi->abi_version < 6) {
        PyErr_SetString(PyExc_RuntimeError, "fs_read unavailable");
        return nullptr;
    }
    size_t l_out_length = 0;
    const char *l_data = s_ffi->fs_read(s_active_plugin->owner_id.constData(), size_t(s_active_plugin->owner_id.size()),
                                        l_path, size_t(l_path_length), &l_out_length);
    return PyBytes_FromStringAndSize(l_data, Py_ssize_t(l_out_length));
}

static PyObject *pyApiFsWrite(PyObject *, PyObject *f_args)
{
    PyObject *l_path_obj = nullptr;
    const char *l_data = nullptr;
    Py_ssize_t l_data_length = 0;
    if (!PyArg_ParseTuple(f_args, "Uy#", &l_path_obj, &l_data, &l_data_length)) {
        return nullptr;
    }
    const char *l_path = nullptr;
    Py_ssize_t l_path_length = 0;
    if (!pyStringArg(l_path_obj, &l_path, &l_path_length)) {
        return nullptr;
    }
    if (!s_active_plugin || s_ffi->abi_version < 6) {
        PyErr_SetString(PyExc_RuntimeError, "fs_write unavailable");
        return nullptr;
    }
    const int l_ok = s_ffi->fs_write(s_active_plugin->owner_id.constData(), size_t(s_active_plugin->owner_id.size()),
                                     l_path, size_t(l_path_length), l_data, size_t(l_data_length));
    return PyBool_FromLong(l_ok);
}

static PyObject *pyApiFsExists(PyObject *, PyObject *f_args)
{
    PyObject *l_path_obj = nullptr;
    if (!PyArg_ParseTuple(f_args, "U", &l_path_obj)) {
        return nullptr;
    }
    const char *l_path = nullptr;
    Py_ssize_t l_path_length = 0;
    if (!pyStringArg(l_path_obj, &l_path, &l_path_length)) {
        return nullptr;
    }
    if (!s_active_plugin || s_ffi->abi_version < 6) {
        PyErr_SetString(PyExc_RuntimeError, "fs_exists unavailable");
        return nullptr;
    }
    const int l_exists = s_ffi->fs_exists(s_active_plugin->owner_id.constData(), size_t(s_active_plugin->owner_id.size()),
                                          l_path, size_t(l_path_length));
    return PyBool_FromLong(l_exists);
}

// Reads an optional Python sequence of str into parallel arrays, kept alive
// by f_storage.
static bool pyCollectParams(PyObject *f_seq, QList<QByteArray> &f_storage,
                            std::vector<const char *> &f_ptrs, std::vector<size_t> &f_lengths)
{
    if (!f_seq || f_seq == Py_None) {
        return true;
    }
    // Stable-ABI sequence access: no PySequence_Fast macros under the
    // limited API. GetItem returns a new reference each time.
    const Py_ssize_t l_count = PySequence_Size(f_seq);
    if (l_count < 0) {
        return false;
    }
    for (Py_ssize_t i = 0; i < l_count; i++) {
        PyObject *l_item = PySequence_GetItem(f_seq, i);
        Py_ssize_t l_length = 0;
        const char *l_text = l_item ? PyUnicode_AsUTF8AndSize(l_item, &l_length) : nullptr;
        f_storage.append(QByteArray(l_text ? l_text : "", int(l_length)));
        Py_XDECREF(l_item);
    }
    for (const QByteArray &l_param : f_storage) {
        f_ptrs.push_back(l_param.constData());
        f_lengths.push_back(size_t(l_param.size()));
    }
    return true;
}

static PyObject *pyApiSqlExec(PyObject *, PyObject *f_args)
{
    PyObject *l_sql_obj = nullptr, *l_params = nullptr;
    if (!PyArg_ParseTuple(f_args, "U|O", &l_sql_obj, &l_params)) {
        return nullptr;
    }
    const char *l_sql = nullptr;
    Py_ssize_t l_sql_length = 0;
    if (!pyStringArg(l_sql_obj, &l_sql, &l_sql_length)) {
        return nullptr;
    }
    if (!s_active_plugin || s_ffi->abi_version < 6) {
        PyErr_SetString(PyExc_RuntimeError, "sql_exec unavailable");
        return nullptr;
    }
    QList<QByteArray> l_storage;
    std::vector<const char *> l_ptrs;
    std::vector<size_t> l_lengths;
    if (!pyCollectParams(l_params, l_storage, l_ptrs, l_lengths)) {
        return nullptr;
    }
    const int l_affected = s_ffi->sql_exec(s_active_plugin->owner_id.constData(), size_t(s_active_plugin->owner_id.size()),
                                           l_sql, size_t(l_sql_length), int(l_ptrs.size()),
                                           l_ptrs.empty() ? nullptr : l_ptrs.data(),
                                           l_lengths.empty() ? nullptr : l_lengths.data());
    return PyLong_FromLong(l_affected);
}

// Hands a query row to a Python callable as a dict of column -> value.
static void pySqlRowTrampoline(void *f_userdata, int f_count,
                               const char *const *f_columns, const char *const *f_values)
{
    PyObject *l_callback = static_cast<PyObject *>(f_userdata);
    PyObject *l_dict = PyDict_New();
    for (int i = 0; i < f_count; i++) {
        PyObject *l_value = PyUnicode_FromString(f_values[i]);
        PyDict_SetItemString(l_dict, f_columns[i], l_value);
        Py_DECREF(l_value);
    }
    PyObject *l_result = PyObject_CallFunctionObjArgs(l_callback, l_dict, nullptr);
    Py_DECREF(l_dict);
    if (l_result) {
        Py_DECREF(l_result);
    }
    else {
        PyErr_Print();
    }
}

static PyObject *pyApiSqlQuery(PyObject *, PyObject *f_args)
{
    PyObject *l_sql_obj = nullptr, *l_callback = nullptr, *l_params = nullptr;
    if (!PyArg_ParseTuple(f_args, "UO|O", &l_sql_obj, &l_callback, &l_params)) {
        return nullptr;
    }
    if (!PyCallable_Check(l_callback)) {
        PyErr_SetString(PyExc_TypeError, "sql_query: second argument must be callable");
        return nullptr;
    }
    const char *l_sql = nullptr;
    Py_ssize_t l_sql_length = 0;
    if (!pyStringArg(l_sql_obj, &l_sql, &l_sql_length)) {
        return nullptr;
    }
    if (!s_active_plugin || s_ffi->abi_version < 6) {
        PyErr_SetString(PyExc_RuntimeError, "sql_query unavailable");
        return nullptr;
    }
    QList<QByteArray> l_storage;
    std::vector<const char *> l_ptrs;
    std::vector<size_t> l_lengths;
    if (!pyCollectParams(l_params, l_storage, l_ptrs, l_lengths)) {
        return nullptr;
    }
    const int l_rows = s_ffi->sql_query(s_active_plugin->owner_id.constData(), size_t(s_active_plugin->owner_id.size()),
                                        l_sql, size_t(l_sql_length), int(l_ptrs.size()),
                                        l_ptrs.empty() ? nullptr : l_ptrs.data(),
                                        l_lengths.empty() ? nullptr : l_lengths.data(),
                                        pySqlRowTrampoline, l_callback);
    return PyLong_FromLong(l_rows);
}

// Runs the migration body: the Python callable held as userdata.
static int pyMigrationTrampoline(void *f_userdata)
{
    PyObject *l_callback = static_cast<PyObject *>(f_userdata);
    PyObject *l_result = PyObject_CallFunctionObjArgs(l_callback, nullptr);
    if (!l_result) {
        PyErr_Print();
        return 0;
    }
    const int l_ok = PyObject_IsTrue(l_result);
    Py_DECREF(l_result);
    return l_ok;
}

static PyObject *pyApiSqlMigrate(PyObject *, PyObject *f_args)
{
    int l_version = 0;
    PyObject *l_callback = nullptr;
    if (!PyArg_ParseTuple(f_args, "iO", &l_version, &l_callback)) {
        return nullptr;
    }
    if (!PyCallable_Check(l_callback)) {
        PyErr_SetString(PyExc_TypeError, "sql_migrate: second argument must be callable");
        return nullptr;
    }
    if (!s_active_plugin || s_ffi->abi_version < 8) {
        PyErr_SetString(PyExc_RuntimeError, "sql_migrate unavailable");
        return nullptr;
    }
    const int l_ok = s_ffi->sql_migrate(s_active_plugin->owner_id.constData(), size_t(s_active_plugin->owner_id.size()),
                                        l_version, pyMigrationTrampoline, l_callback);
    return PyBool_FromLong(l_ok);
}

static PyObject *pyApiSqlRead(PyObject *, PyObject *f_args)
{
    PyObject *l_source_obj = nullptr, *l_sql_obj = nullptr, *l_callback = nullptr, *l_params = nullptr;
    if (!PyArg_ParseTuple(f_args, "UUO|O", &l_source_obj, &l_sql_obj, &l_callback, &l_params)) {
        return nullptr;
    }
    if (!PyCallable_Check(l_callback)) {
        PyErr_SetString(PyExc_TypeError, "sql_read: third argument must be callable");
        return nullptr;
    }
    const char *l_source = nullptr, *l_sql = nullptr;
    Py_ssize_t l_source_length = 0, l_sql_length = 0;
    if (!pyStringArg(l_source_obj, &l_source, &l_source_length) || !pyStringArg(l_sql_obj, &l_sql, &l_sql_length)) {
        return nullptr;
    }
    if (s_ffi->abi_version < 8) {
        PyErr_SetString(PyExc_RuntimeError, "sql_read unavailable");
        return nullptr;
    }
    QList<QByteArray> l_storage;
    std::vector<const char *> l_ptrs;
    std::vector<size_t> l_lengths;
    if (!pyCollectParams(l_params, l_storage, l_ptrs, l_lengths)) {
        return nullptr;
    }
    const int l_rows = s_ffi->sql_read(l_source, size_t(l_source_length), l_sql, size_t(l_sql_length),
                                       int(l_ptrs.size()),
                                       l_ptrs.empty() ? nullptr : l_ptrs.data(),
                                       l_lengths.empty() ? nullptr : l_lengths.data(),
                                       pySqlRowTrampoline, l_callback);
    return PyLong_FromLong(l_rows);
}

static PyObject *pyApiScheduleRepeating(PyObject *, PyObject *f_args)
{
    PyObject *l_job_obj = nullptr, *l_day_obj = nullptr, *l_time_obj = nullptr, *l_handler = nullptr;
    if (!PyArg_ParseTuple(f_args, "UUUO", &l_job_obj, &l_day_obj, &l_time_obj, &l_handler)) {
        return nullptr;
    }
    if (!PyCallable_Check(l_handler)) {
        PyErr_SetString(PyExc_TypeError, "schedule_repeating: last argument must be callable");
        return nullptr;
    }
    const char *l_job = nullptr, *l_day = nullptr, *l_time = nullptr;
    Py_ssize_t l_job_length = 0, l_day_length = 0, l_time_length = 0;
    if (!pyStringArg(l_job_obj, &l_job, &l_job_length) || !pyStringArg(l_day_obj, &l_day, &l_day_length) || !pyStringArg(l_time_obj, &l_time, &l_time_length)) {
        return nullptr;
    }
    if (s_ffi->abi_version < 9) {
        PyErr_SetString(PyExc_RuntimeError, "schedule_repeating unavailable");
        return nullptr;
    }
    PyFnRef *l_ref = takeFnRef(l_handler);
    if (!l_ref) {
        return nullptr;
    }
    return PyBool_FromLong(s_ffi->schedule_repeating(l_ref->plugin->owner_id.constData(), size_t(l_ref->plugin->owner_id.size()),
                                                     l_job, size_t(l_job_length), l_day, size_t(l_day_length),
                                                     l_time, size_t(l_time_length), pyConsoleTrampoline, l_ref));
}

static PyObject *pyApiScheduleOnce(PyObject *, PyObject *f_args)
{
    PyObject *l_job_obj = nullptr, *l_when_obj = nullptr, *l_handler = nullptr;
    if (!PyArg_ParseTuple(f_args, "UUO", &l_job_obj, &l_when_obj, &l_handler)) {
        return nullptr;
    }
    if (!PyCallable_Check(l_handler)) {
        PyErr_SetString(PyExc_TypeError, "schedule_once: last argument must be callable");
        return nullptr;
    }
    const char *l_job = nullptr, *l_when = nullptr;
    Py_ssize_t l_job_length = 0, l_when_length = 0;
    if (!pyStringArg(l_job_obj, &l_job, &l_job_length) || !pyStringArg(l_when_obj, &l_when, &l_when_length)) {
        return nullptr;
    }
    if (s_ffi->abi_version < 9) {
        PyErr_SetString(PyExc_RuntimeError, "schedule_once unavailable");
        return nullptr;
    }
    PyFnRef *l_ref = takeFnRef(l_handler);
    if (!l_ref) {
        return nullptr;
    }
    return PyBool_FromLong(s_ffi->schedule_once(l_ref->plugin->owner_id.constData(), size_t(l_ref->plugin->owner_id.size()),
                                                l_job, size_t(l_job_length), l_when, size_t(l_when_length),
                                                pyConsoleTrampoline, l_ref));
}

static PyObject *pyApiScheduleCancel(PyObject *, PyObject *f_args)
{
    PyObject *l_job_obj = nullptr;
    if (!PyArg_ParseTuple(f_args, "U", &l_job_obj)) {
        return nullptr;
    }
    const char *l_job = nullptr;
    Py_ssize_t l_job_length = 0;
    if (!pyStringArg(l_job_obj, &l_job, &l_job_length)) {
        return nullptr;
    }
    if (!s_active_plugin || s_ffi->abi_version < 9) {
        PyErr_SetString(PyExc_RuntimeError, "schedule_cancel unavailable");
        return nullptr;
    }
    s_ffi->schedule_cancel(s_active_plugin->owner_id.constData(), size_t(s_active_plugin->owner_id.size()),
                           l_job, size_t(l_job_length));
    Py_RETURN_NONE;
}

static PyObject *pyApiScheduleNextRun(PyObject *, PyObject *f_args)
{
    PyObject *l_job_obj = nullptr;
    if (!PyArg_ParseTuple(f_args, "U", &l_job_obj)) {
        return nullptr;
    }
    const char *l_job = nullptr;
    Py_ssize_t l_job_length = 0;
    if (!pyStringArg(l_job_obj, &l_job, &l_job_length)) {
        return nullptr;
    }
    if (!s_active_plugin || s_ffi->abi_version < 9) {
        PyErr_SetString(PyExc_RuntimeError, "schedule_next_run unavailable");
        return nullptr;
    }
    size_t l_out_length = 0;
    const char *l_next = s_ffi->schedule_next_run(s_active_plugin->owner_id.constData(), size_t(s_active_plugin->owner_id.size()),
                                                  l_job, size_t(l_job_length), &l_out_length);
    return PyUnicode_FromStringAndSize(l_next, Py_ssize_t(l_out_length));
}

static PyObject *pyApiAreaGet(PyObject *, PyObject *f_args)
{
    int l_area_id = 0;
    PyObject *l_key_obj = nullptr;
    if (!PyArg_ParseTuple(f_args, "iU", &l_area_id, &l_key_obj)) {
        return nullptr;
    }
    const char *l_key = nullptr;
    Py_ssize_t l_key_length = 0;
    if (!pyStringArg(l_key_obj, &l_key, &l_key_length)) {
        return nullptr;
    }
    if (s_ffi->abi_version < 10) {
        PyErr_SetString(PyExc_RuntimeError, "area_get unavailable");
        return nullptr;
    }
    size_t l_out_length = 0;
    const char *l_value = s_ffi->area_get(l_area_id, l_key, size_t(l_key_length), &l_out_length);
    return PyUnicode_FromStringAndSize(l_value, Py_ssize_t(l_out_length));
}

static PyObject *pyApiAreaSet(PyObject *, PyObject *f_args)
{
    int l_area_id = 0;
    PyObject *l_key_obj = nullptr, *l_value_obj = nullptr;
    if (!PyArg_ParseTuple(f_args, "iUU", &l_area_id, &l_key_obj, &l_value_obj)) {
        return nullptr;
    }
    const char *l_key = nullptr, *l_value = nullptr;
    Py_ssize_t l_key_length = 0, l_value_length = 0;
    if (!pyStringArg(l_key_obj, &l_key, &l_key_length) || !pyStringArg(l_value_obj, &l_value, &l_value_length)) {
        return nullptr;
    }
    if (s_ffi->abi_version < 10) {
        PyErr_SetString(PyExc_RuntimeError, "area_set unavailable");
        return nullptr;
    }
    return PyBool_FromLong(s_ffi->area_set(l_area_id, l_key, size_t(l_key_length), l_value, size_t(l_value_length)));
}

static PyObject *pyApiFloorGet(PyObject *, PyObject *f_args)
{
    int l_floor_id = 0;
    PyObject *l_key_obj = nullptr;
    if (!PyArg_ParseTuple(f_args, "iU", &l_floor_id, &l_key_obj)) {
        return nullptr;
    }
    const char *l_key = nullptr;
    Py_ssize_t l_key_length = 0;
    if (!pyStringArg(l_key_obj, &l_key, &l_key_length)) {
        return nullptr;
    }
    if (s_ffi->abi_version < 10) {
        PyErr_SetString(PyExc_RuntimeError, "floor_get unavailable");
        return nullptr;
    }
    size_t l_out_length = 0;
    const char *l_value = s_ffi->floor_get(l_floor_id, l_key, size_t(l_key_length), &l_out_length);
    return PyUnicode_FromStringAndSize(l_value, Py_ssize_t(l_out_length));
}

static PyObject *pyApiWorldAreaCount(PyObject *, PyObject *)
{
    return PyLong_FromLong(s_ffi->abi_version < 10 ? 0 : s_ffi->world_area_count());
}

static PyObject *pyApiWorldFloorCount(PyObject *, PyObject *)
{
    return PyLong_FromLong(s_ffi->abi_version < 10 ? 0 : s_ffi->world_floor_count());
}

// Hands an outbound packet to a Python interceptor as (header, [fields]).
// Return False to drop it, a list of strings to replace the fields, else it
// passes unchanged.
static int pyInterceptorTrampoline(void *f_userdata,
                                   const char *f_header, size_t f_header_length,
                                   int f_field_count, const char *const *f_fields, const size_t *f_field_lengths,
                                   AkashiPacketResult *f_result)
{
    PyFnRef *l_ref = static_cast<PyFnRef *>(f_userdata);
    PythonPluginState *l_previous = s_active_plugin;
    s_active_plugin = l_ref->plugin;

    PyObject *l_fields = PyList_New(f_field_count);
    for (int i = 0; i < f_field_count; i++) {
        PyList_SetItem(l_fields, i, PyUnicode_FromStringAndSize(f_fields[i], Py_ssize_t(f_field_lengths[i])));
    }
    PyObject *l_header = PyUnicode_FromStringAndSize(f_header, Py_ssize_t(f_header_length));
    PyObject *l_ret = PyObject_CallFunctionObjArgs(l_ref->callable, l_header, l_fields, nullptr);
    Py_DECREF(l_header);
    Py_DECREF(l_fields);
    s_active_plugin = l_previous;

    int l_verdict = 1;
    if (!l_ret) {
        PyErr_Print();
        return 1; // an erroring interceptor lets the packet through
    }
    if (l_ret == Py_False) {
        l_verdict = 0;
    }
    else if (PyList_Check(l_ret)) {
        const Py_ssize_t l_count = PyList_Size(l_ret);
        std::vector<QByteArray> l_storage;
        std::vector<const char *> l_ptrs;
        std::vector<size_t> l_lens;
        l_storage.reserve(l_count);
        for (Py_ssize_t i = 0; i < l_count; i++) {
            Py_ssize_t l_len = 0;
            const char *l_str = PyUnicode_AsUTF8AndSize(PyList_GetItem(l_ret, i), &l_len);
            l_storage.emplace_back(l_str ? l_str : "", int(l_len));
        }
        for (const QByteArray &l_field : l_storage) {
            l_ptrs.push_back(l_field.constData());
            l_lens.push_back(size_t(l_field.size()));
        }
        s_ffi->packet_result_set(f_result, f_header, f_header_length, int(l_count),
                                 l_ptrs.empty() ? nullptr : l_ptrs.data(),
                                 l_lens.empty() ? nullptr : l_lens.data());
    }
    Py_DECREF(l_ret);
    return l_verdict;
}

static PyObject *pyApiRegisterOutboundInterceptor(PyObject *, PyObject *f_args)
{
    PyObject *l_header_obj = nullptr, *l_handler = nullptr;
    int l_order = 0;
    if (!PyArg_ParseTuple(f_args, "OiO", &l_header_obj, &l_order, &l_handler)) {
        return nullptr;
    }
    if (!PyCallable_Check(l_handler)) {
        PyErr_SetString(PyExc_TypeError, "register_outbound_interceptor: last argument must be callable");
        return nullptr;
    }
    const char *l_header = "";
    Py_ssize_t l_header_length = 0;
    if (l_header_obj != Py_None && !pyStringArg(l_header_obj, &l_header, &l_header_length)) {
        return nullptr;
    }
    if (s_ffi->abi_version < 10) {
        PyErr_SetString(PyExc_RuntimeError, "register_outbound_interceptor unavailable");
        return nullptr;
    }
    PyFnRef *l_ref = takeFnRef(l_handler);
    if (!l_ref) {
        return nullptr;
    }
    return PyBool_FromLong(s_ffi->register_outbound_interceptor(l_header, size_t(l_header_length), l_order,
                                                                pyInterceptorTrampoline, l_ref,
                                                                l_ref->plugin->owner_id.constData(), size_t(l_ref->plugin->owner_id.size())));
}

// True when the active plugin can drive the discord verbs, else sets a
// Python error and returns false.
static bool pyDiscordReady(const char *f_verb)
{
    if (!s_active_plugin || s_ffi->abi_version < 7) {
        PyErr_Format(PyExc_RuntimeError, "%s unavailable", f_verb);
        return false;
    }
    return true;
}

static const char *pyOwner(size_t *f_length)
{
    *f_length = size_t(s_active_plugin->owner_id.size());
    return s_active_plugin->owner_id.constData();
}

static PyObject *pyApiDiscordBegin(PyObject *, PyObject *)
{
    if (!pyDiscordReady("discord_begin")) {
        return nullptr;
    }
    size_t l_owner_length = 0;
    const char *l_owner = pyOwner(&l_owner_length);
    s_ffi->discord_begin(l_owner, l_owner_length);
    Py_RETURN_NONE;
}

static PyObject *pyApiDiscordSet(PyObject *, PyObject *f_args)
{
    const char *l_key = nullptr, *l_value = nullptr;
    Py_ssize_t l_key_length = 0, l_value_length = 0;
    if (!PyArg_ParseTuple(f_args, "s#s#", &l_key, &l_key_length, &l_value, &l_value_length)) {
        return nullptr;
    }
    if (!pyDiscordReady("discord_set")) {
        return nullptr;
    }
    size_t l_owner_length = 0;
    const char *l_owner = pyOwner(&l_owner_length);
    s_ffi->discord_set(l_owner, l_owner_length, l_key, size_t(l_key_length), l_value, size_t(l_value_length));
    Py_RETURN_NONE;
}

static PyObject *pyApiDiscordEmbedBegin(PyObject *, PyObject *)
{
    if (!pyDiscordReady("discord_embed_begin")) {
        return nullptr;
    }
    size_t l_owner_length = 0;
    const char *l_owner = pyOwner(&l_owner_length);
    s_ffi->discord_embed_begin(l_owner, l_owner_length);
    Py_RETURN_NONE;
}

static PyObject *pyApiDiscordEmbedSet(PyObject *, PyObject *f_args)
{
    const char *l_key = nullptr, *l_value = nullptr;
    Py_ssize_t l_key_length = 0, l_value_length = 0;
    if (!PyArg_ParseTuple(f_args, "s#s#", &l_key, &l_key_length, &l_value, &l_value_length)) {
        return nullptr;
    }
    if (!pyDiscordReady("discord_embed_set")) {
        return nullptr;
    }
    size_t l_owner_length = 0;
    const char *l_owner = pyOwner(&l_owner_length);
    s_ffi->discord_embed_set(l_owner, l_owner_length, l_key, size_t(l_key_length), l_value, size_t(l_value_length));
    Py_RETURN_NONE;
}

static PyObject *pyApiDiscordEmbedFooter(PyObject *, PyObject *f_args)
{
    const char *l_text = nullptr, *l_icon = "";
    Py_ssize_t l_text_length = 0, l_icon_length = 0;
    if (!PyArg_ParseTuple(f_args, "s#|s#", &l_text, &l_text_length, &l_icon, &l_icon_length)) {
        return nullptr;
    }
    if (!pyDiscordReady("discord_embed_footer")) {
        return nullptr;
    }
    size_t l_owner_length = 0;
    const char *l_owner = pyOwner(&l_owner_length);
    s_ffi->discord_embed_footer(l_owner, l_owner_length, l_text, size_t(l_text_length), l_icon, size_t(l_icon_length));
    Py_RETURN_NONE;
}

static PyObject *pyApiDiscordEmbedAuthor(PyObject *, PyObject *f_args)
{
    const char *l_name = nullptr, *l_url = "", *l_icon = "";
    Py_ssize_t l_name_length = 0, l_url_length = 0, l_icon_length = 0;
    if (!PyArg_ParseTuple(f_args, "s#|s#s#", &l_name, &l_name_length, &l_url, &l_url_length, &l_icon, &l_icon_length)) {
        return nullptr;
    }
    if (!pyDiscordReady("discord_embed_author")) {
        return nullptr;
    }
    size_t l_owner_length = 0;
    const char *l_owner = pyOwner(&l_owner_length);
    s_ffi->discord_embed_author(l_owner, l_owner_length, l_name, size_t(l_name_length),
                                l_url, size_t(l_url_length), l_icon, size_t(l_icon_length));
    Py_RETURN_NONE;
}

static PyObject *pyApiDiscordEmbedField(PyObject *, PyObject *f_args)
{
    const char *l_name = nullptr, *l_value = nullptr;
    Py_ssize_t l_name_length = 0, l_value_length = 0;
    int l_inline = 0;
    if (!PyArg_ParseTuple(f_args, "s#s#|p", &l_name, &l_name_length, &l_value, &l_value_length, &l_inline)) {
        return nullptr;
    }
    if (!pyDiscordReady("discord_embed_field")) {
        return nullptr;
    }
    size_t l_owner_length = 0;
    const char *l_owner = pyOwner(&l_owner_length);
    s_ffi->discord_embed_field(l_owner, l_owner_length, l_name, size_t(l_name_length),
                               l_value, size_t(l_value_length), l_inline);
    Py_RETURN_NONE;
}

static PyObject *pyApiDiscordEmbedEnd(PyObject *, PyObject *)
{
    if (!pyDiscordReady("discord_embed_end")) {
        return nullptr;
    }
    size_t l_owner_length = 0;
    const char *l_owner = pyOwner(&l_owner_length);
    s_ffi->discord_embed_end(l_owner, l_owner_length);
    Py_RETURN_NONE;
}

static PyObject *pyApiDiscordPost(PyObject *, PyObject *f_args)
{
    const char *l_url = nullptr;
    Py_ssize_t l_url_length = 0;
    if (!PyArg_ParseTuple(f_args, "s#", &l_url, &l_url_length)) {
        return nullptr;
    }
    if (!pyDiscordReady("discord_post")) {
        return nullptr;
    }
    size_t l_owner_length = 0;
    const char *l_owner = pyOwner(&l_owner_length);
    return PyBool_FromLong(s_ffi->discord_post(l_owner, l_owner_length, l_url, size_t(l_url_length)));
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
    {"grant", pyApiGrant, METH_VARARGS, "grant(permission, audience, key): a standing grant to a person (key = IPID) or role (key = role id)."},
    {"revoke", pyApiRevoke, METH_VARARGS, "revoke(permission, audience, key): removes exactly the matching grant this plugin placed."},
    {"register_rule_action", pyApiRegisterRuleAction, METH_VARARGS, "register_rule_action(name, phase, handler); a before handler may return a str or False to block."},
    {"register_console_action", pyApiRegisterConsoleAction, METH_VARARGS, "register_console_action(title, handler): puts a task on the server console's menu."},
    {"subscribe_event", pyApiSubscribeEvent, METH_VARARGS, "subscribe_event(name, handler); the handler receives the payload dict."},
    {"publish_event", pyApiPublishEvent, METH_VARARGS, "publish_event(name, payload_dict)."},
    {"config_get", pyApiConfigGet, METH_VARARGS, "config_get(key, fallback='') from the plugin's config file."},
    {"config_set", pyApiConfigSet, METH_VARARGS, "config_set(key, value) into the plugin's config file."},
    {"config_declare", pyApiConfigDeclare, METH_VARARGS, "config_declare(key, type, default='', description=''): typed, validated setting."},
    {"fs_read", pyApiFsRead, METH_VARARGS, "fs_read(path) -> bytes, from the plugin's own data folder."},
    {"fs_write", pyApiFsWrite, METH_VARARGS, "fs_write(path, data: bytes) -> bool, into the plugin's data folder."},
    {"fs_exists", pyApiFsExists, METH_VARARGS, "fs_exists(path) -> bool, within the plugin's data folder."},
    {"sql_exec", pyApiSqlExec, METH_VARARGS, "sql_exec(sql, params=()) -> affected rows, on the plugin's database."},
    {"sql_query", pyApiSqlQuery, METH_VARARGS, "sql_query(sql, row_fn, params=()) -> row count; row_fn receives a dict per row."},
    {"sql_migrate", pyApiSqlMigrate, METH_VARARGS, "sql_migrate(version, fn) -> bool; runs fn once to reach the schema version."},
    {"sql_read", pyApiSqlRead, METH_VARARGS, "sql_read(source, sql, row_fn, params=()) -> row count; read-only query of 'main' or a plugin id."},
    {"schedule_repeating", pyApiScheduleRepeating, METH_VARARGS, "schedule_repeating(job_id, day, time, fn): day is 'daily' or a weekday, time is 'HH:MM'."},
    {"schedule_once", pyApiScheduleOnce, METH_VARARGS, "schedule_once(job_id, when, fn): when is a duration '1d12h', a weekday, or a date."},
    {"schedule_cancel", pyApiScheduleCancel, METH_VARARGS, "schedule_cancel(job_id)."},
    {"schedule_next_run", pyApiScheduleNextRun, METH_VARARGS, "schedule_next_run(job_id) -> 'yyyy-MM-dd hh:mm' or ''."},
    {"area_get", pyApiAreaGet, METH_VARARGS, "area_get(area_id, key) -> str; read an area property."},
    {"area_set", pyApiAreaSet, METH_VARARGS, "area_set(area_id, key, value) -> bool; set a bounded area property."},
    {"floor_get", pyApiFloorGet, METH_VARARGS, "floor_get(floor_id, key) -> str."},
    {"world_area_count", pyApiWorldAreaCount, METH_NOARGS, "world_area_count() -> int."},
    {"world_floor_count", pyApiWorldFloorCount, METH_NOARGS, "world_floor_count() -> int."},
    {"register_outbound_interceptor", pyApiRegisterOutboundInterceptor, METH_VARARGS, "register_outbound_interceptor(header, order, fn): fn(header, fields) -> False|list|None."},
    {"discord_begin", pyApiDiscordBegin, METH_NOARGS, "discord_begin() starts a Discord message draft."},
    {"discord_set", pyApiDiscordSet, METH_VARARGS, "discord_set(key, value): content/username/avatar_url/tts."},
    {"discord_embed_begin", pyApiDiscordEmbedBegin, METH_NOARGS, "discord_embed_begin() opens an embed."},
    {"discord_embed_set", pyApiDiscordEmbedSet, METH_VARARGS, "discord_embed_set(key, value): title/description/url/color/timestamp/image/thumbnail."},
    {"discord_embed_footer", pyApiDiscordEmbedFooter, METH_VARARGS, "discord_embed_footer(text, icon_url='')."},
    {"discord_embed_author", pyApiDiscordEmbedAuthor, METH_VARARGS, "discord_embed_author(name, url='', icon_url='')."},
    {"discord_embed_field", pyApiDiscordEmbedField, METH_VARARGS, "discord_embed_field(name, value, inline=False)."},
    {"discord_embed_end", pyApiDiscordEmbedEnd, METH_NOARGS, "discord_embed_end() closes the embed."},
    {"discord_post", pyApiDiscordPost, METH_VARARGS, "discord_post(url) -> bool, sends the draft through the core hook."},
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
    // Scripts live in the host's own subfolder, created on first run -
    // only files in there are this host's to load.
    QList<akashi::PluginInfo> discoverScriptPlugins(const QString &f_plugin_dir) override
    {
        QList<akashi::PluginInfo> l_manifests;
        const QString l_script_dir = f_plugin_dir + QStringLiteral("/python");
        QDir().mkpath(l_script_dir);
        const QDir l_dir(l_script_dir);
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
akashi::ServiceVersion PythonHostPlugin::pluginVersion() const { return {1, 2, 0}; }

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
    // The manifest cannot know which Python the server runs on, so the
    // credit line is composed here and overrides it.
    if (auto l_plugins = services.resolve<akashi::PluginManager>(QStringLiteral("akashi.plugins"))) {
        l_plugins->registerAbout(id(), "Runs the server's .py plugins on Python " +
                                           QString::fromUtf8(Py_GetVersion()).section(' ', 0, 0) +
                                           " by the Python Software Foundation (PSF license).");
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
