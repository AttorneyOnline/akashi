#pragma once

#include "akashi/service.h"

#include <QList>
#include <QString>

namespace akashi {

struct PluginInfo;

// The seam a scripting host provides so the plugin manager can run plugins
// written in that host's language. A host registers its implementation as
// "akashi.script-host.<runtime>" - the lua host under akashi.script-host.lua.
//
// The host owns everything language-specific, INCLUDING FINDING ITS OWN
// PLUGIN FILES: after the native plugins start (and again whenever a new
// host loads at runtime), the manager asks every registered script host to
// scan the plugin directory and report manifests. What a plugin file looks
// like is the host's business - the bundled hosts read single script files
// with an embedded declaration header, a host for a compiled language could
// read library sidecars instead.
//
// Contract for the reported manifests: ids must be unique, the entry_path
// must be loadable by loadScriptPlugin, and the dependencies must contain
// the host's own plugin id, so the plugins order after their host and
// cascade with it.
class IScriptPluginHost : public IService
{
  public:
    // The runtime name this host provides ("lua", "python").
    virtual QString runtime() const = 0;

    // Scans f_plugin_dir for this host's plugin files and returns their
    // manifests. Called after every native plugin started, again when a
    // host loads at runtime, and to refresh one plugin's manifest before a
    // reload. Must tolerate repeated calls.
    virtual QList<PluginInfo> discoverScriptPlugins(const QString &f_plugin_dir) = 0;

    // Runs the plugin's entry. Everything the script registers must carry
    // f_plugin_id as its owner, so the plugin lists, unloads and sweeps
    // like any other.
    virtual bool loadScriptPlugin(const QString &f_plugin_id, const QString &f_entry_path) = 0;

    // Stops the plugin's script and drops its handlers. The manager sweeps
    // the plugin's registrations before calling this.
    virtual void unloadScriptPlugin(const QString &f_plugin_id) = 0;
};

} // namespace akashi
