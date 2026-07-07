cmake_minimum_required(VERSION 3.19)
# Builds in the akashi tree or standalone against an installed akashi;
# see plugins/README.md.
if(NOT TARGET akashi::akashi_pluginapi)
    project(akashi-python-host LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_AUTOMOC ON)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Core)
    qt_standard_project_setup()
    find_package(Python3 REQUIRED COMPONENTS Development)
    find_package(akashi CONFIG REQUIRED)
else()
    include(${CMAKE_SOURCE_DIR}/cmake/akashiPlugin.cmake)
endif()

akashi_add_plugin(akashi_python_host
    SOURCES
        python_host_plugin.h
        python_host_plugin.cpp
)
target_include_directories(akashi_python_host PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../scripting-ffi
    ${Python3_INCLUDE_DIRS})

# The host builds against the stable ABI (python3, not python3XX), so one
# built plugin works across Python 3.x versions.
if(MSVC)
    # pyconfig.h auto-links python3.lib under Py_LIMITED_API; the linker
    # only needs the folder.
    if(NOT Python3_LIBRARY_DIRS)
        get_filename_component(Python3_LIBRARY_DIRS "${Python3_EXECUTABLE}" DIRECTORY)
        set(Python3_LIBRARY_DIRS "${Python3_LIBRARY_DIRS}/libs")
    endif()
    target_link_directories(akashi_python_host PRIVATE ${Python3_LIBRARY_DIRS})
else()
    # Windows auto-links the python3 stable-ABI lib; Linux has no
    # unversioned libpython3.so, so link the found interpreter lib.
    target_link_libraries(akashi_python_host PRIVATE ${Python3_LIBRARIES})
endif()
