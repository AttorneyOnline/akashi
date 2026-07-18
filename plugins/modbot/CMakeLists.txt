cmake_minimum_required(VERSION 3.19)
# Builds in the akashi tree or standalone against an installed akashi;
# see plugins/README.md.
if(NOT TARGET akashi::akashi_pluginapi)
    project(akashi-modbot LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_AUTOMOC ON)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Core Sql)
    qt_standard_project_setup()
    find_package(akashi CONFIG REQUIRED)
else()
    include(${CMAKE_SOURCE_DIR}/cmake/akashiPlugin.cmake)
endif()

akashi_add_plugin(akashi_modbot
    SOURCES
        modbot_plugin.h
        modbot_plugin.cpp
        modbot_rules.h
        modbot_rules.cpp
        modbot_worker.h
        modbot_worker.cpp
)

target_link_libraries(akashi_modbot PRIVATE Qt6::Sql)
