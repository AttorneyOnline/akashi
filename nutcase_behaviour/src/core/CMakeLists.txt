add_library(akashi_core SHARED
    include/packet.h
    include/service.h
    include/serviceregistry.h src/serviceregistry.cpp
    include/akashi_core_global.h
)

add_library(akashi::Core ALIAS akashi_core)


target_compile_definitions(akashi_core PRIVATE
    AKASHI_CORE_LIBRARY
)

target_include_directories(akashi_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(akashi_core
    PUBLIC
        Qt6::Core
)
