add_library(akashi_core SHARED

    include/service.h
    include/serviceregistry.h src/serviceregistry.cpp
    include/akashi_core_global.h
    include/akashiplugin.h
    include/servicewrapper.h
    include/packet.h
    src/packet.cpp
    include/packetregistry.h
    src/packetregistry.cpp
)

add_library(akashi::Core ALIAS akashi_core)

set_target_properties(akashi_core PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY $<1:$${CMAKE_SOURCE_DIR}/bin>
        RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_SOURCE_DIR}/bin>
)

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

install(TARGETS akashi_core
    EXPORT akashi_core-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/core
    FILES_MATCHING PATTERN "*.h"
)

install(EXPORT akashi_core-targets
    FILE akashi_core-targets.cmake
    NAMESPACE akashi::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/akashi
)
