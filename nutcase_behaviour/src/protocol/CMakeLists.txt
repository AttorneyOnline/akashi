add_library(akashi_protocol SHARED
    include/akashi_protocol_global.h
)

add_library(akashi::Protocol ALIAS akashi_protocol)

set_target_properties(akashi_protocol PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY $<1:$${CMAKE_SOURCE_DIR}/bin>
        RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_SOURCE_DIR}/bin>
)

target_compile_definitions(akashi_protocol PRIVATE
    AKASHI_PROTOCOL_LIBRARY
)

target_include_directories(akashi_protocol
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(akashi_protocol
    PUBLIC
        Qt6::Core
)

install(TARGETS akashi_protocol
    EXPORT akashi_protocol-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/protocol
    FILES_MATCHING PATTERN "*.h"
)

install(EXPORT akashi_protocol-targets
    FILE akashi_protocol-targets.cmake
    NAMESPACE akashi::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/akashi
)
