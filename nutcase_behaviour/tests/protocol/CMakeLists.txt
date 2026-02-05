# Protocol library tests

# Test for packet_ms (MS packet implementations)
qt_add_executable(test_packet_ms test_packet_ms.cpp)
target_link_libraries(test_packet_ms PRIVATE Qt6::Test akashi::Core akashi::Protocol)
add_test(NAME ProtocolPacketMSTest COMMAND test_packet_ms)

# Set output directory for test binaries
set_target_properties(
    test_packet_ms
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/protocol
)
