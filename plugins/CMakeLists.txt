# Selects the bundled plugins that get built and deployed to bin/plugins.
# Every switch is a cache option, so the set is chosen on the configure line:
#   cmake -DAKASHI_BUILD_PLUGINS=OFF ..                the whole set off
#   cmake -DAKASHI_PLUGIN_DISCORD_INTEGRATION=OFF ..   one plugin off
# Disabling a plugin only stops building it; a library an earlier build
# already deployed stays in bin/plugins until removed by hand.

option(AKASHI_BUILD_PLUGINS "Build the bundled plugins" ON)

# One folder per plugin, one AKASHI_PLUGIN_* option per folder
# (hello-world becomes AKASHI_PLUGIN_HELLO_WORLD).
set(AKASHI_BUNDLED_PLUGINS
    discord-integration
    hello-world
    lua-host
    medieval-speak
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

# Each enabled script host ships its sample scripts next to the built
# plugin libraries.
set(_script_samples "")
if("lua-host" IN_LIST _enabled_plugins)
    list(APPEND _script_samples
        ${CMAKE_CURRENT_SOURCE_DIR}/script-samples/hello-lua.lua
        ${CMAKE_CURRENT_SOURCE_DIR}/script-samples/showcase-lua.lua)
endif()
if("python-host" IN_LIST _enabled_plugins)
    list(APPEND _script_samples
        ${CMAKE_CURRENT_SOURCE_DIR}/script-samples/hello-python.py
        ${CMAKE_CURRENT_SOURCE_DIR}/script-samples/showcase-python.py)
endif()
if(_script_samples)
    add_custom_target(script_samples ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/bin/plugins
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_script_samples} ${CMAKE_SOURCE_DIR}/bin/plugins)
endif()
