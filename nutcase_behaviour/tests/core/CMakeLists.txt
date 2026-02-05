# Core library tests

# Test for Packet class
qt_add_executable(test_packet test_packet.cpp)
target_link_libraries(test_packet PRIVATE Qt6::Test akashi::Core)
add_test(NAME CorePacketTest COMMAND test_packet)

# Test for PacketHelpers namespace
qt_add_executable(test_packet_helpers test_packet_helpers.cpp)
target_link_libraries(test_packet_helpers PRIVATE Qt6::Test akashi::Core)
add_test(NAME CorePacketHelpersTest COMMAND test_packet_helpers)

# Test for PacketDecoder
qt_add_executable(test_packet_decoder test_packet_decoder.cpp)
target_link_libraries(test_packet_decoder PRIVATE Qt6::Test akashi::Core)
add_test(NAME CorePacketDecoderTest COMMAND test_packet_decoder)

# Test for PacketBuilder
qt_add_executable(test_packet_builder test_packet_builder.cpp)
target_link_libraries(test_packet_builder PRIVATE Qt6::Test akashi::Core)
add_test(NAME CorePacketBuilderTest COMMAND test_packet_builder)

# Test for PacketRegistry
qt_add_executable(test_packetregistry test_packetregistry.cpp)
target_link_libraries(test_packetregistry PRIVATE Qt6::Test akashi::Core)
add_test(NAME CorePacketRegistryTest COMMAND test_packetregistry)

# Test for ServiceRegistry
qt_add_executable(test_serviceregistry test_serviceregistry.cpp)
target_link_libraries(test_serviceregistry PRIVATE Qt6::Test akashi::Core)
add_test(NAME CoreServiceRegistryTest COMMAND test_serviceregistry)

# Test for ServiceWrapper
qt_add_executable(test_servicewrapper test_servicewrapper.cpp)
target_link_libraries(test_servicewrapper PRIVATE Qt6::Test akashi::Core)
add_test(NAME CoreServiceWrapperTest COMMAND test_servicewrapper)

# Set output directory for test binaries
set_target_properties(
    test_packet
    test_packet_helpers
    test_packet_decoder
    test_packet_builder
    test_packetregistry
    test_serviceregistry
    test_servicewrapper
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/core
)
