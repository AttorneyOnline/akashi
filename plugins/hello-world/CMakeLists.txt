include(${CMAKE_SOURCE_DIR}/cmake/akashiPlugin.cmake)

akashi_add_plugin(akashi_hello_world
    SOURCES
        hello_world_plugin.h
        hello_world_plugin.cpp
)
