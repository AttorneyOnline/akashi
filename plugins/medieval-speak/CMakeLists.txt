include(${CMAKE_SOURCE_DIR}/cmake/akashiPlugin.cmake)
akashi_add_plugin(akashi_medieval_speak SOURCES
    medieval_speak_plugin.cpp
    medieval_speak_plugin.h
    medieval_parser.cpp
    medieval_parser.h)
