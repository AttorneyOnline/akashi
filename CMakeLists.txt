cmake_minimum_required(VERSION 3.19)
project(akashi VERSION 1.9.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(BUILD_TESTING "Build tests and the miniclient" OFF)

set(_qt_components Core Concurrent Network WebSockets Sql)
if(BUILD_TESTING)
  list(APPEND _qt_components Test)
endif()
find_package(Qt6 6.4 REQUIRED COMPONENTS ${_qt_components})

qt_standard_project_setup()

# Software identity for the boot screen and the ID packet. Stamped at
# configure time; a stale branch/commit refreshes on the next CMake run.
set(AKASHI_CODENAME "jackfruit")
execute_process(COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE AKASHI_GIT_BRANCH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE AKASHI_GIT_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT AKASHI_GIT_BRANCH)
  set(AKASHI_GIT_BRANCH "unknown")
endif()
if(NOT AKASHI_GIT_COMMIT)
  set(AKASHI_GIT_COMMIT "unknown")
endif()
if(CMAKE_BUILD_TYPE)
  set(AKASHI_RELEASE_TYPE ${CMAKE_BUILD_TYPE})
else()
  set(AKASHI_RELEASE_TYPE "Development")
endif()
configure_file(src/core/softwareinformation.h.in softwareinformation.h)

# The public headers a plugin may use, and nothing else: the akashi_sdk
# interface target below carries src/include, and each layer library
# carries its own include/ directory. Everything else in src/ is private
# to the core build.
set(AKASHI_SOURCES
  src/core/event_bus.cpp
  src/core/include/core/event_bus.h
  src/core/log_service.cpp
  src/core/include/core/log_service.h
  src/core/writer_text.cpp
  src/core/writer_text.h
  src/include/akashi/event.h
  src/include/akashi/log_event.h
  src/include/akashi/log_writer.h
  src/core/console_input.cpp
  src/core/include/core/console_input.h
  src/core/console_log.cpp
  src/core/console_log.h
  src/core/console_menu.cpp
  src/core/include/core/console_menu.h
  src/core/console_panel.cpp
  src/core/console_panel.h
  src/core/logging_categories.cpp
  src/core/include/core/logging_categories.h
  src/core/crypto_helper.h
  src/core/db_manager.cpp
  src/core/db_manager.h
  src/core/player_directory.cpp
  src/core/player_directory.h
  src/core/player_state_observer.cpp
  src/core/player_state_observer.h
  src/core/server_publisher.cpp
  src/core/server_publisher.h
  src/core/rule_actions.cpp
  src/core/rule_actions.h
  src/core/arup_broadcaster.cpp
  src/core/arup_broadcaster.h
  src/core/config_entry.cpp
  src/core/config_store.cpp
  src/core/database_service.cpp
  src/core/exit_code.h
  src/core/network_service.cpp
  src/core/service.cpp
  src/core/service_registry.cpp
  src/core/filesystem_service.cpp
  src/core/json_settings.cpp
  src/core/json_settings.h
  src/core/client_session.cpp
  src/core/client_session.h
  src/core/command_context.cpp
  src/core/include/core/command_context.h
  src/core/command_registry.cpp
  src/core/include/core/command_registry.h
  src/core/include/core/command_spec.h
  src/core/permission_registry.cpp
  src/core/include/core/permission_registry.h
  src/core/auth_throttle.cpp
  src/core/auth_throttle.h
  src/core/plugin_manager.cpp
  src/core/include/core/plugin_manager.h
  src/core/text_filter_registry.cpp
  src/core/include/core/text_filter_registry.h
  src/core/mmdb_reader.cpp
  src/core/mmdb_reader.h
  src/core/player_state.cpp
  src/core/player_state.h
  src/core/server_settings.h
  src/core/settings.cpp
  src/core/server_context.cpp
  src/core/server_context.h
  src/core/websocket_receiver.cpp
  src/core/websocket_receiver.h
  src/include/akashi/client_receiver.h
  src/commands/area_commands.cpp
  src/commands/area_commands.h
  src/commands/authentication_commands.cpp
  src/commands/authentication_commands.h
  src/commands/moderation_commands.cpp
  src/commands/moderation_commands.h
  src/commands/casing_commands.cpp
  src/commands/casing_commands.h
  src/commands/music_commands.cpp
  src/commands/music_commands.h
  src/commands/plugin_commands.cpp
  src/commands/plugin_commands.h
  src/commands/roleplay_commands.cpp
  src/commands/roleplay_commands.h
  src/commands/rule_commands.cpp
  src/commands/rule_commands.h
  src/commands/messaging_commands.cpp
  src/commands/messaging_commands.h
  src/include/akashi/area_rule.h
  src/include/akashi/config_entry.h
  src/include/akashi/config_store.h
  src/include/akashi/setting_notifier.h
  src/include/akashi/database_service.h
  src/include/akashi/filesystem_service.h
  src/include/akashi/jukebox_policy.h
  src/include/akashi/network_service.h
  src/include/akashi/permissions.h
  src/include/akashi/sanctions.h
  src/include/akashi/script_plugin_host.h
  src/include/akashi/service.h
  src/include/akashi/text_filter.h
  src/include/akashi/thread_assert.h
  src/include/akashi/plugin.h
  src/include/akashi/service_registry.h
  src/include/akashi/settings.h
)

# The dedicated public header target: the SDK headers plus the generated
# export header. Every library and plugin gets its includes through this
# or through a layer library's own include/ directory - never through the
# private src/ tree.
add_library(akashi_sdk INTERFACE)
target_include_directories(akashi_sdk INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
    "$<INSTALL_INTERFACE:include>"
)
target_link_libraries(akashi_sdk INTERFACE Qt6::Core)
# The whole project uses Q_EMIT/Q_SIGNALS/Q_SLOTS instead of the lowercase keywords.
target_compile_definitions(akashi_sdk INTERFACE QT_NO_KEYWORDS)

# The protocol and world layers: static libraries folded into the
# akashi_core library, each with its own public include directory.
add_subdirectory(src/proto)
add_subdirectory(src/world)

add_library(akashi_core SHARED ${AKASHI_SOURCES})
include(GenerateExportHeader)
generate_export_header(akashi_core BASE_NAME AKASHI_CORE)
target_include_directories(akashi_core
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/core/include>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/proto/include>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/world/include>"
        "$<INSTALL_INTERFACE:include>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# What a plugin builds against: the SDK headers plus the public faces of
# the core, protocol and world layers - and only those. A plugin including
# a private header fails to compile, so the API boundary holds itself.
add_library(akashi_pluginapi INTERFACE)
target_include_directories(akashi_pluginapi INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/include>"
    "$<INSTALL_INTERFACE:include>"
)
target_link_libraries(akashi_pluginapi INTERFACE akashi_core)

# In-tree plugins consume the exact target names an installed akashi
# exports, so any bundled plugin also builds standalone via find_package.
add_library(akashi::akashi_core ALIAS akashi_core)
add_library(akashi::akashi_sdk ALIAS akashi_sdk)
add_library(akashi::akashi_pluginapi ALIAS akashi_pluginapi)
target_link_libraries(akashi_core
    PUBLIC
        akashi_sdk
        Qt6::Concurrent
        Qt6::Core
        Qt6::Sql
        Qt6::Network
        Qt6::WebSockets
    PRIVATE
        akashi_proto
        akashi_world
)

qt_add_executable(akashi src/main.cpp)
target_link_libraries(akashi PRIVATE akashi_core)
# The executable is part of the core build and may use its private headers.
target_include_directories(akashi PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

set_target_properties(akashi PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

# The terminal that attaches to a running server's console menu.
qt_add_executable(akashi-console tools/akashi-console/main.cpp)
target_link_libraries(akashi-console PRIVATE akashi_core)
set_target_properties(akashi-console PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_CURRENT_LIST_DIR}/bin>)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  if(WIN32)
    set_property(TARGET akashi PROPERTY WIN32_EXECUTABLE true)
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/data/icon/akashi.rc")
    target_sources(akashi PRIVATE ${APP_ICON_RESOURCE_WINDOWS})
    set_target_properties(akashi PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:CONSOLE /ENTRY:mainCRTStartup")
  endif()
  target_compile_definitions(akashi_core PRIVATE QT_NO_DEBUG_OUTPUT)
  target_compile_definitions(akashi PRIVATE QT_NO_DEBUG_OUTPUT)
endif()

# The $<1:...> wrapper keeps multi-config generators from adding per-config subfolders.
set_target_properties(akashi PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY $<1:${CMAKE_CURRENT_LIST_DIR}/bin>
        RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_CURRENT_LIST_DIR}/bin>)

# The server loads the core library from its own folder.
set_target_properties(akashi_core PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY $<1:${CMAKE_CURRENT_LIST_DIR}/bin>
        RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_CURRENT_LIST_DIR}/bin>)

# Install rules, so plugins can later use find_package(akashi).
include(CMakePackageConfigHelpers)
install(TARGETS akashi_core akashi_sdk akashi_pluginapi EXPORT akashiTargets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)
install(DIRECTORY src/include/ DESTINATION include)
install(DIRECTORY src/proto/include/ DESTINATION include)
install(DIRECTORY src/world/include/ DESTINATION include)
install(DIRECTORY src/core/include/ DESTINATION include)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/akashi_core_export.h DESTINATION include)
install(EXPORT akashiTargets NAMESPACE akashi:: DESTINATION lib/cmake/akashi)
configure_package_config_file(cmake/akashiConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/akashiConfig.cmake
    INSTALL_DESTINATION lib/cmake/akashi)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/akashiConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/akashiConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/akashiConfigVersion.cmake
    cmake/akashiPlugin.cmake
    DESTINATION lib/cmake/akashi)

if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(plugins/discord-integration)
  add_subdirectory(plugins/hello-world)
  add_subdirectory(plugins/medieval-speak)
  add_subdirectory(plugins/sql-logger)
  add_subdirectory(plugins/scripting-ffi)
  add_subdirectory(plugins/lua-host)
  find_package(Python3 COMPONENTS Interpreter Development QUIET)
  if(Python3_Development_FOUND)
    add_subdirectory(plugins/python-host)
  else()
    message(STATUS "Python development files not found; the python-host plugin is skipped.")
  endif()
  # The sample script plugins ship next to the built plugin libraries.
  add_custom_target(script_samples ALL
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_SOURCE_DIR}/plugins/script-samples/hello-lua.lua
          ${CMAKE_SOURCE_DIR}/plugins/script-samples/hello-python.py
          ${CMAKE_SOURCE_DIR}/plugins/script-samples/showcase-lua.lua
          ${CMAKE_SOURCE_DIR}/plugins/script-samples/showcase-python.py
          ${CMAKE_SOURCE_DIR}/bin/plugins)
  add_subdirectory(tests)
endif()
