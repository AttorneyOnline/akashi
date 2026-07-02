# Writing akashi plugins in Lua and Python

An akashi script plugin is **one file**. Drop a `.lua` or `.py` file into
`bin/plugins/` next to the compiled plugins, list its id in
`config/plugins.ini`, and the server discovers, orders and loads it like
any other plugin. No C++, no build step, no extra files.

The samples in this folder are working plugins:

| File | What it shows |
|---|---|
| `hello-lua.lua` / `hello-python.py` | The smallest useful plugin: a command, a filter, an event |
| `showcase-lua.lua` / `showcase-python.py` | The whole surface: permissions, rules, targets, config, console tasks |

## The declaration header

The first thing in the file is a comment holding the word `akashi-plugin`
followed by a JSON object. That object is the plugin's manifest.

Lua:

```lua
--[[ akashi-plugin
{
    "id": "akashi.my-plugin",
    "version": "1.0.0"
}
--]]
```

Python (a module docstring):

```python
"""akashi-plugin
{
    "id": "akashi.my-plugin",
    "version": "1.0.0"
}
"""
import akashi
```

Every key is optional:

| Key | Default | Meaning |
|---|---|---|
| `id` | the file name | The plugin id used in `plugins.ini`, `/plugin` commands and as the owner of everything the plugin registers |
| `version` | `1.0.0` | Shown in `/plugin list` and the load messages |
| `runtime` | from the extension | `lua` or `python`; only needed for unusual file names |
| `dependencies` | none | Plugin ids that must load first. The matching host (`akashi.lua-host` / `akashi.python-host`) is always added for you |
| `services` | none | Service ids the plugin expects; validated before load |

## Lifecycle

The file runs top to bottom when the plugin loads — registrations happen
at load time, handlers fire later. Everything a plugin registers is owned
by its id: when the plugin unloads (`/plugin unload`, a dependency
cascade, or server shutdown), its commands, filters, events, permissions,
rules and console tasks are all removed automatically. `/plugin reload`
re-reads both the file and its header. Lua plugins each get their own
interpreter state; Python plugins each get their own module namespace with
the full standard library of the installed interpreter (any Python 3.10 or
newer works — the bridge uses the stable ABI).

Everything runs on the server's main thread. Handlers should return
quickly; a slow handler stalls the whole server.

## The API

Both languages see the same functions — Lua as the global `akashi` table,
Python as the `akashi` module (`import akashi`). The only convention
difference: **Lua indexes command arguments from 1, Python from 0**, each
matching its language.

### Commands

```lua
akashi.register_command(name, usage, description, handler, permission, min_args)
```

`permission` (default none) and `min_args` (default 0) are optional. The
handler receives `(ctx, args)`: an opaque context for the invoker and the
argument list. `ctx` is only valid during the call — never store it.

Reading the invoker: `client_id(ctx)`, `player_name(ctx)`,
`character(ctx)`, `area_name(ctx)`, `area_id(ctx)`,
`is_authenticated(ctx)`, `can_perform(ctx, permission)`.

Answering: `reply(ctx, text)` to the invoker, `reply_to_area(ctx, text)`
to everyone in their area.

Acting on a player named by an argument (the target verbs take the
argument's position): `target_id(ctx, index)` (the client id, or -1),
`target_reply(ctx, index, text)`, `target_has_sanction(ctx, index, id)`,
`target_set_sanction(ctx, index, id, active)`,
`target_change_area(ctx, index, area_id)`.

### Text filters

```lua
akashi.register_text_filter(id, order, always_active, handler)
```

Filters rewrite IC chat. Lower `order` runs earlier (the built-in curses
sit around 300). With `always_active` false, the filter runs only for
players whose sanction set holds `id` — that is how a curse works: declare
the filter, then toggle the sanction with `target_set_sanction`. The
handler receives the text and returns a rewritten string, `false` to drop
the message entirely, or nothing to pass it through unchanged.

### Events

```lua
akashi.subscribe_event(name, handler)
akashi.publish_event(name, payload)
```

Handlers run after the event happened and receive a key/value payload
(a table in Lua, a dict in Python; values are strings). The core events:
`modcall`, `ban_issued`, `kick_issued`, `ic_message`, `ooc_message`,
`player_joined_area`, `player_left_area`, `area_changed`,
`music_changed`, `evidence_presented`, `command_executed`,
`config_reloaded`. Any other name is a custom event — publish one and
every plugin subscribed to that name hears it, which is how script
plugins talk to each other.

### Permissions

```lua
akashi.register_permission(id, display_name, category)
```

Declares a permission id so role files can grant it and commands can gate
on it. Namespace your ids (`myplugin.curse`), and remember the standing
rule: a permission a role grants must never be taken away by area rules
or config.

### Area rules

```lua
akashi.register_rule_action(name, phase, handler)
```

Registers a named action for the area rule system; owners attach it to
floors and areas with `/addrule`, `/floorrule` or `areas.json`, passing
`key=value` arguments. `phase` is `"before"` (gates the event) or
`"after"` (reacts to it). The handler receives one table/dict with
`player_id`, `area_id`, `floor_id`, `payload` (the event's data) and
`args` (the arguments the rule was attached with). A before handler
blocks the event by returning a reason string (or `False` in Python).
Applied rules are swept with the plugin, so nothing dangles after an
unload.

### The server console

```lua
akashi.register_console_action(title, handler)
akashi.console_print(text)
```

Console actions appear in the tasks view of the server's operator menu —
on the server's own terminal, over the `akashi-console` attach client,
wherever they are attached; outside a task it goes to the server log.
`akashi.log(text)` always writes to the server log.

### Configuration

```lua
akashi.config_get(key, fallback)
```

Reads from the plugin's own file, `config/plugins/<id>.json` — flat JSON
keys, created by the owner. Values come back as strings.

## Writing a host for another language

The bundled hosts are ordinary plugins, and a third party can add another
runtime the same way:

1. Implement `akashi::IScriptPluginHost` (`akashi/script_plugin_host.h`)
   in a small C++ plugin and register it as the service
   `akashi.script-host.<runtime>`. Discovery is the host's own business —
   `discoverScriptPlugins(dir)` decides what a plugin file looks like
   (the bundled hosts use `PluginManager::parseScriptHeader` for the
   declaration-header format above, but a host may do anything).
2. Depend on `akashi.scripting-ffi` and consume its C table
   (`plugins/scripting-ffi/akashi_ffi.h`): a flat, append-only struct of
   plain C functions — no Qt types, UTF-8 with explicit lengths — made
   for `bindgen`-style tooling. Check `abi_version` before using late
   entries; returned strings are valid until the next FFI call, so copy
   immediately.
3. Register everything under the script plugin's id, and tear the
   interpreter down only after the manager's cleanup ran (the manager
   sweeps registries before it asks the host to unload the script).

For compiled languages the natural shape is a generic native host that
loads shared libraries exporting an init function taking the table — a
plugin-sized project that needs no changes to akashi itself.
