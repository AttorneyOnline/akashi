# Protocol regression tests, see tst_protocol.cpp.
qt_add_executable(tst_protocol tst_protocol.cpp)

target_link_libraries(tst_protocol PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Test
    Qt6::WebSockets
)

target_compile_definitions(tst_protocol PRIVATE
    AKASHI_BINARY_PATH="$<TARGET_FILE:akashi>"
    AKASHI_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

add_test(NAME protocol COMMAND tst_protocol)
set_tests_properties(protocol PROPERTIES TIMEOUT 180)

if(WIN32)
    add_custom_command(TARGET tst_protocol POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:akashi_core> $<TARGET_FILE_DIR:tst_protocol>)
endif()
