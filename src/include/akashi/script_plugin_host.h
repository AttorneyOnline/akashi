#pragma once

#include "akashi/service.h"

namespace akashi {

// The seam a scripting host provides so the plugin manager can run plugins
// written in that host's language. A host registers its implementation as
// "akashi.script-host.<runtime>" - the lua host under akashi.script-host.lua.
//
// A script plugin is a folder in the plugins directory holding a plugin.json
// manifest and an entry script; the manifest needs only an id, a version and
// the entry file. The manager infers the runtime from the entry's extension
// and adds the host plugin as a dependency by itself, so a script author
// never touches C++ or the build.
class IScriptPluginHost : public IService
{
  public:
    // The runtime name entry extensions map to ("lua", "python").
    virtual QString runtime() const = 0;

    // Runs the plugin's entry script. Everything the script registers must
    // carry f_plugin_id as its owner, so the plugin lists, unloads and
    // sweeps like any other.
    virtual bool loadScriptPlugin(const QString &f_plugin_id, const QString &f_entry_path) = 0;

    // Stops the plugin's script and drops its handlers.
    virtual void unloadScriptPlugin(const QString &f_plugin_id) = 0;
};

} // namespace akashi
