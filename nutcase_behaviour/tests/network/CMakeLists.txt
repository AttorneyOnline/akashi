# Network library tests

# Test for DiscordHook
qt_add_executable(test_discordhook test_discordhook.cpp)
target_link_libraries(test_discordhook PRIVATE Qt6::Test Qt6::Network akashi::Core akashi::Network)
add_test(NAME NetworkDiscordHookTest COMMAND test_discordhook)

# Test for ServerPublisher
qt_add_executable(test_serverpublisher test_serverpublisher.cpp)
target_link_libraries(test_serverpublisher PRIVATE Qt6::Test Qt6::Network akashi::Core akashi::Network)
add_test(NAME NetworkServerPublisherTest COMMAND test_serverpublisher)

# Set output directory for test binaries
set_target_properties(
    test_discordhook
    test_serverpublisher
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/network
)
