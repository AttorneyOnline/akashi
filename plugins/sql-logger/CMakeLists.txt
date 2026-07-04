cmake_minimum_required(VERSION 3.19)
# Builds in the akashi tree or standalone against an installed akashi;
# see plugins/README.md.
if(NOT TARGET akashi::akashi_pluginapi)
    project(akashi-sql-logger LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_AUTOMOC ON)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Core)
    qt_standard_project_setup()
    find_package(akashi CONFIG REQUIRED)
else()
    include(${CMAKE_SOURCE_DIR}/cmake/akashiPlugin.cmake)
endif()

akashi_add_plugin(akashi_sql_logger
    SOURCES
        sql_logger_plugin.h
        sql_logger_plugin.cpp
        writer_sql.h
        writer_sql.cpp
)

target_link_libraries(akashi_sql_logger PRIVATE Qt6::Sql)
