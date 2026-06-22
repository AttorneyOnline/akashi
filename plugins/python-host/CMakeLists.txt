include(${CMAKE_SOURCE_DIR}/cmake/akashiPlugin.cmake)

akashi_add_plugin(akashi_python_host
    SOURCES
        python_host_plugin.h
        python_host_plugin.cpp
)
target_include_directories(akashi_python_host PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/scripting-ffi
    ${Python3_INCLUDE_DIRS})
target_link_libraries(akashi_python_host PRIVATE ${Python3_LIBRARIES})
