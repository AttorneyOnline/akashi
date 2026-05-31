function(akashi_add_plugin name)
    cmake_parse_arguments(PLUGIN "" "" "SOURCES" ${ARGN})
    qt_add_plugin(${name} SHARED ${PLUGIN_SOURCES})
    target_link_libraries(${name} PRIVATE akashi_pluginapi)
    target_compile_definitions(${name} PRIVATE QT_NO_KEYWORDS)
    set_target_properties(${name} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY $<1:${CMAKE_SOURCE_DIR}/bin/plugins>
        RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_SOURCE_DIR}/bin/plugins>)
endfunction()
