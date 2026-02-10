add_library(akashi_addon SHARED
    include/akashi_addon_global.h
    include/service.h src/service.cpp
    include/serviceregistry.h src/serviceregistry.cpp
    include/servicewrapper.h src/servicewrapper.cpp
    include/discordhook.h src/discordhook.cpp
)
add_library(akashi::Addon ALIAS akashi_addon)

set_target_properties(akashi_addon PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY $<1:${CMAKE_SOURCE_DIR}/bin>
        RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_SOURCE_DIR}/bin>
)

target_compile_definitions(akashi_addon PRIVATE
    AKASHI_ADDON_LIBRARY
)

target_include_directories(akashi_addon
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(akashi_addon
    PUBLIC
        Qt6::Core
        Qt6::Network
)
