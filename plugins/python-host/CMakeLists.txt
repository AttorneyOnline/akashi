include(${CMAKE_SOURCE_DIR}/cmake/akashiPlugin.cmake)

akashi_add_plugin(akashi_python_host
    SOURCES
        python_host_plugin.h
        python_host_plugin.cpp
)
target_include_directories(akashi_python_host PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/scripting-ffi
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
    target_link_libraries(akashi_python_host PRIVATE python3)
endif()
