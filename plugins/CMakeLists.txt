# Selects the bundled plugins that get built and deployed to bin/plugins.
# Every switch is a cache option, so the set is chosen on the configure line:
#   cmake -DAKASHI_BUILD_PLUGINS=OFF ..                the whole set off
#   cmake -DAKASHI_PLUGIN_DISCORD_INTEGRATION=OFF ..   one plugin off
# Disabling a plugin only stops building it; a library an earlier build
# already deployed stays in bin/plugins until removed by hand.

option(AKASHI_BUILD_PLUGINS "Build the bundled plugins" ON)

# One folder per plugin, one AKASHI_PLUGIN_* option per folder
# (lua-host becomes AKASHI_PLUGIN_LUA_HOST). The samples folder and the
# per-host sample scripts are reference material, never built or deployed.
set(AKASHI_BUNDLED_PLUGINS
    discord-integration
    lua-host
    python-host
    scripting-ffi
    sql-logger
)

set(_enabled_plugins "")
foreach(_plugin ${AKASHI_BUNDLED_PLUGINS})
    string(TOUPPER ${_plugin} _option)
    string(REPLACE "-" "_" _option ${_option})
    option(AKASHI_PLUGIN_${_option} "Build the bundled ${_plugin} plugin" ON)
    if(AKASHI_BUILD_PLUGINS AND AKASHI_PLUGIN_${_option})
        list(APPEND _enabled_plugins ${_plugin})
    endif()
endforeach()

# The python host embeds CPython, so it also needs the development files.
if("python-host" IN_LIST _enabled_plugins)
    find_package(Python3 COMPONENTS Interpreter Development QUIET)
    if(NOT Python3_Development_FOUND)
        message(STATUS "Python development files not found; the python-host plugin is skipped.")
        list(REMOVE_ITEM _enabled_plugins python-host)
    endif()
endif()

# The script hosts declare akashi.scripting-ffi as a runtime dependency;
# they build without it, but the server will refuse to load them.
foreach(_host lua-host python-host)
    if("${_host}" IN_LIST _enabled_plugins AND NOT "scripting-ffi" IN_LIST _enabled_plugins)
        message(WARNING "${_host} is enabled without scripting-ffi and will not load at runtime.")
    endif()
endforeach()

foreach(_plugin ${_enabled_plugins})
    add_subdirectory(${_plugin})
endforeach()

if(_enabled_plugins)
    list(JOIN _enabled_plugins ", " _enabled_pretty)
    message(STATUS "Bundled plugins: ${_enabled_pretty}")
else()
    message(STATUS "Bundled plugins: none")
endif()
