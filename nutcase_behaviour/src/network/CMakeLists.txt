add_library(akashi_network SHARED
    include/akashi_network_global.h
    include/packet.h    src/packet.cpp
    include/packetfactory.h
    src/packetfactory.cpp
)

add_library(akashi::Network ALIAS akashi_network)

set_target_properties(akashi_network PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY $<1:${CMAKE_SOURCE_DIR}/bin>
        RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_SOURCE_DIR}/bin>
)

target_compile_definitions(akashi_network PRIVATE
    AKASHI_NETWORK_LIBRARY
)

target_include_directories(akashi_network
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(akashi_network
    PUBLIC
        akashi::Core
        Qt6::Network
)

install(TARGETS akashi_network
    EXPORT akashi_network-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/network
    FILES_MATCHING PATTERN "*.h"
)

install(EXPORT akashi_network-targets
    FILE akashi_network-targets.cmake
    NAMESPACE akashi::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/akashi
)
