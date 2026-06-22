include(${CMAKE_SOURCE_DIR}/cmake/akashiPlugin.cmake)

akashi_add_plugin(akashi_scripting_ffi
    SOURCES
        akashi_ffi.h
        scripting_ffi_service.h
        scripting_ffi_plugin.h
        scripting_ffi_plugin.cpp
)
