cmake_minimum_required(VERSION 3.19)
# Builds in the akashi tree or standalone against an installed akashi;
# see plugins/README.md.
if(NOT TARGET akashi::akashi_pluginapi)
    project(akashi-medieval-speak LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_AUTOMOC ON)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Core)
    qt_standard_project_setup()
    find_package(akashi CONFIG REQUIRED)
else()
    include(${CMAKE_SOURCE_DIR}/cmake/akashiPlugin.cmake)
endif()
akashi_add_plugin(akashi_medieval_speak SOURCES
    medieval_speak_plugin.cpp
    medieval_speak_plugin.h
    medieval_parser.cpp
    medieval_parser.h)
