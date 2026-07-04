cmake_minimum_required(VERSION 3.19)
# Builds in the akashi tree or standalone against an installed akashi;
# see plugins/README.md.
if(NOT TARGET akashi::akashi_pluginapi)
    project(akashi-lua-host LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_AUTOMOC ON)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Core)
    qt_standard_project_setup()
    find_package(akashi CONFIG REQUIRED)
else()
    include(${CMAKE_SOURCE_DIR}/cmake/akashiPlugin.cmake)
endif()

# The Lua interpreter, compiled in as plain C. The sources are fetched
# from lua.org at configure time into lua/, which is gitignored - the
# interpreter is a build input, not part of this repository.
set(LUA_VERSION 5.4.8)
set(LUA_SHA256 4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae)
set(LUA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lua)
if(NOT EXISTS ${LUA_DIR}/lua.h)
    set(_lua_tarball ${CMAKE_CURRENT_BINARY_DIR}/lua-${LUA_VERSION}.tar.gz)
    message(STATUS "Fetching Lua ${LUA_VERSION} from lua.org")
    file(DOWNLOAD https://www.lua.org/ftp/lua-${LUA_VERSION}.tar.gz ${_lua_tarball}
        EXPECTED_HASH SHA256=${LUA_SHA256}
        STATUS _lua_download)
    list(GET _lua_download 0 _lua_error_code)
    if(NOT _lua_error_code EQUAL 0)
        list(GET _lua_download 1 _lua_error_text)
        message(FATAL_ERROR "Could not download Lua ${LUA_VERSION}: ${_lua_error_text}")
    endif()
    file(ARCHIVE_EXTRACT INPUT ${_lua_tarball} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(GLOB _lua_files
        ${CMAKE_CURRENT_BINARY_DIR}/lua-${LUA_VERSION}/src/*.c
        ${CMAKE_CURRENT_BINARY_DIR}/lua-${LUA_VERSION}/src/*.h)
    file(COPY ${_lua_files} DESTINATION ${LUA_DIR})
endif()

enable_language(C)
file(GLOB LUA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/lua/*.c)
list(REMOVE_ITEM LUA_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/lua/lua.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lua/luac.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lua/onelua.c)

akashi_add_plugin(akashi_lua_host
    SOURCES
        lua_host_plugin.h
        lua_host_plugin.cpp
        ${LUA_SOURCES}
)
target_include_directories(akashi_lua_host PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../scripting-ffi)
