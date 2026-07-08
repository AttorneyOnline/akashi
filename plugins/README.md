# Writing akashi plugins in C++

A native akashi plugin is a Qt plugin library: one `plugin.json` manifest,
one class implementing `akashi::IPlugin`, built against the public akashi
headers and dropped into `bin/plugins/`. This guide covers how to get
those headers, how to build against them in and out of the akashi tree,
and what the API offers. Plugins in Lua or Python need none of this —
see [`script-samples/README.md`](script-samples/README.md).

## How akashi deploys its headers

The build defines the public API as CMake targets, and `cmake --install`
deploys exactly that surface — private headers are never installed, so
the installed tree *is* the API:

```
<prefix>/
  bin/akashi_core.dll            the core library
  lib/akashi_core.lib            its import library
  lib/cmake/akashi/              find_package(akashi) config, targets,
                                 and the akashi_add_plugin() helper
  include/
    akashi_core_export.h         the export macro header
    akashi/                      the SDK: service, plugin, event, filter,
                                 rule, transport and settings interfaces
    core/                        core's public face: command registry and
                                 context, event bus, log service, console
                                 menu, permission and text filter
                                 registries, plugin manager
    proto/                       the protocol layer: packet value type,
                                 codecs, packet registry, ITransport
    world/                       the world model: areas, floors, jukebox,
                                 evidence, testimony, rules
```

Producing that tree from a source checkout:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=C:/Qt/6.11.0/msvc2022_64
cmake --build build
cmake --install build --prefix C:/dev/akashi-sdk
```

Inside the build, the same boundary holds without installing anything:
each layer is a CMake target owning its own `include/` directory
(`src/proto/include`, `src/world/include`, `src/core/include`, and the
`akashi_sdk` interface target for `src/include`), and plugin targets can
only see those. Including a private header is a compile error either way.

## Consuming akashi from your own repository

A complete plugin project is four files. `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.19)
project(greeter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core)
qt_standard_project_setup()

find_package(akashi CONFIG REQUIRED)

akashi_add_plugin(akashi_greeter
    SOURCES
        greeter_plugin.h
        greeter_plugin.cpp
)
```

`find_package(akashi)` brings in the installed targets (namespaced
`akashi::akashi_core`, `akashi::akashi_sdk`, `akashi::akashi_pluginapi`)
plus the Qt modules akashi itself needs, and `akashi_add_plugin()` wires
the plugin: it links `akashi::akashi_pluginapi`, enforces the
Q_EMIT/Q_SIGNALS keyword style, and puts the built library in your
project's `bin/plugins/`. Configure pointing at the deployed tree and
your Qt:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug ^
      "-DCMAKE_PREFIX_PATH=C:/dev/akashi-sdk;C:/Qt/6.11.0/msvc2022_64"
cmake --build build
```

Match the build type of the akashi you installed (a Debug package serves
Debug plugins). Deploying is copying the library into the server's
`bin/plugins/` and listing the plugin id in `config/plugins.ini`.

`plugin.json`, compiled into the library and read by the server before
loading:

```json
{
    "id": "akashi.greeter",
    "version": "1.0.0",
    "dependencies": [],
    "services": ["akashi.commands"]
}
```

`dependencies` are plugin ids that must load first (their services exist
when yours loads); `services` are the service ids you expect, validated
before your code runs.

`greeter_plugin.h`:

```cpp
#pragma once

#include "akashi/plugin.h"

#include <QObject>
#include <QtPlugin>

class GreeterPlugin : public QObject, public akashi::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID AkashiPlugin_iid FILE "plugin.json")
    Q_INTERFACES(akashi::IPlugin)

  public:
    QString id() const override;
    akashi::ServiceVersion pluginVersion() const override;

    bool load(akashi::ServiceRegistry &services) override;
    void shutdown(akashi::ServiceRegistry &services) override;
};
```

`greeter_plugin.cpp`:

```cpp
#include "greeter_plugin.h"

#include "akashi/service_registry.h"
#include "core/command_context.h"
#include "core/command_registry.h"
#include "core/command_spec.h"

QString GreeterPlugin::id() const { return QStringLiteral("akashi.greeter"); }
akashi::ServiceVersion GreeterPlugin::pluginVersion() const { return {1, 0, 0}; }

bool GreeterPlugin::load(akashi::ServiceRegistry &services)
{
    auto l_commands = services.resolve<akashi::CommandRegistry>(QStringLiteral("akashi.commands"));
    if (!l_commands) {
        return false;
    }
    l_commands->registerCommand(
        {QStringLiteral("greet"), {}, {}, 0,
         QStringLiteral("/greet"),
         QStringLiteral("Greets you from an out-of-tree plugin.")},
        [](akashi::CommandContext &f_context) {
            f_context.reply(QStringLiteral("Hello from a plugin built outside the akashi tree!"));
        },
        id());
    return true;
}

void GreeterPlugin::shutdown(akashi::ServiceRegistry &services)
{
    Q_UNUSED(services)
}
```

Everything registered under the plugin's id — commands, filters, events,
permissions, rules, console tasks, log writers — is swept automatically
when the plugin unloads, so `shutdown()` only needs to release what the
registries don't own (worker threads, open files, service objects you
registered yourself).

## The bundled plugins are the worked examples

Every folder in this directory is a standalone subproject that consumes
akashi exactly the way the greeter above does — same `plugin.json`, same
`IPlugin` class, same `akashi_add_plugin()` call, linking the same
`akashi::akashi_pluginapi` target name. Each `CMakeLists.txt` opens with
a guard that makes the folder build both ways:

```cmake
if(NOT TARGET akashi::akashi_pluginapi)
    # standalone: bootstrap a project and find the installed akashi
    ...
    find_package(akashi CONFIG REQUIRED)
else()
    # inside the akashi tree: the targets already exist
    include(${CMAKE_SOURCE_DIR}/cmake/akashiPlugin.cmake)
endif()
```

So the quickest start is to copy any bundled plugin folder out of the
tree and build it against your installed akashi — it compiles unchanged.
plugin bringing its own Qt module and config file, `discord-integration`
shows event subscriptions, and the two script hosts show implementing a
service other plugins depend on. To bundle a new plugin with the server
instead, keep the same folder shape and add the folder name to the
`AKASHI_BUNDLED_PLUGINS` list in `plugins/CMakeLists.txt` — that file
gives it an `AKASHI_PLUGIN_*` switch automatically.

## Choosing which bundled plugins to build

`plugins/CMakeLists.txt` builds every bundled plugin by default and
deploys them to `bin/plugins/`. The set is chosen with configure
arguments: `-DAKASHI_BUILD_PLUGINS=OFF` turns the whole set off, and
each plugin has its own switch named after its folder —
`-DAKASHI_PLUGIN_LUA_HOST=OFF` drops `lua-host`, and so on. A server

```
cmake -S . -B build -DAKASHI_PLUGIN_DISCORD_INTEGRATION=OFF -DAKASHI_PLUGIN_RCON_SERVER=OFF
```

The python host is additionally skipped when the CPython development
files are missing, and disabling `scripting-ffi` while keeping a script
host enabled earns a configure warning, because the hosts depend on it
at runtime. Disabling a plugin only stops building it — a library an
earlier build already put in `bin/plugins/` stays there until you
delete it.

## The API a plugin sees

Resolve services from the registry passed to `load()`; never cache them
past `shutdown()`. The core services:

| Service id | Type (header) | What it does |
|---|---|---|
| `akashi.commands` | `CommandRegistry` (`core/command_registry.h`) | Chat commands, aliases, gate-checked variants |
| `akashi.events` | `EventBus` (`core/event_bus.h`) | Typed core events plus custom cross-plugin events |
| `akashi.textfilters` | `TextFilterRegistry` (`core/text_filter_registry.h`) | Ordered IC text rewrite/drop chain |
| `akashi.permissions` | `PermissionRegistry` (`core/permission_registry.h`) | Permission declaration and role queries |
| `akashi.rules` | `RuleRegistry` (`world/rule_registry.h`) | Named actions for the area rule system |
| `akashi.log` | `LogService` (`core/log_service.h`) | Structured log events; register your own writers |
| `akashi.config` | `ConfigStore` (`akashi/config_store.h`) | Your own config file via `declarePlugin()` |
| `akashi.filesystem` | `FileSystemService` (`akashi/filesystem_service.h`) | Path containment and a per-plugin data folder |
| `akashi.database` | `DatabaseService` (`akashi/database_service.h`) | Maintenance scheduling for your SQLite files |
| `akashi.network` | `NetworkService` (`akashi/network_service.h`) | The shared QNetworkAccessManager |
| `akashi.console` | `ConsoleMenu` (`core/console_menu.h`) | Operator menu tasks and sessions |
| `akashi.plugins` | `PluginManager` (`core/plugin_manager.h`) | Plugin listing and the script header parser |
| `akashi.packets` | `PacketService` (`proto/packet_service.h`) | Packet handler and codec registration |
| `akashi.script-host.<runtime>` | `IScriptPluginHost` (`akashi/script_plugin_host.h`) | Implement this to host another scripting language |

Ground rules: everything runs on the server's main thread — lifecycle
calls and every registry callback — so return quickly; spawn your own
worker threads for heavy work and marshal results back with queued
signals. Log through a category so the console's category column stays
filled: include `akashi/logging_categories.h` and use
`qCInfo(akashiPlugins)` or whichever subsystem fits, or declare your
own category. Permissions and sanctions are plain string ids (namespace yours
like `myplugin.curse`); the core ids live in `akashi/permissions.h` and
`akashi/sanctions.h`. A rule or config argument must never take away a
permission a role grants.
