cmake_minimum_required(VERSION 3.19)

# Enable testing
enable_testing()

# Find Qt Test module
find_package(Qt6 REQUIRED COMPONENTS Test)

# Copy all library files to the shared test directory
add_custom_target(copy_test_dlls ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/tests
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:akashi_core>
        ${CMAKE_BINARY_DIR}/tests/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:akashi_network>
        ${CMAKE_BINARY_DIR}/tests/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:akashi_protocol>
        ${CMAKE_BINARY_DIR}/tests/
    DEPENDS akashi_core akashi_network akashi_protocol
    COMMENT "Copying libraries to test directory"
)

# Add test subdirectories
add_subdirectory(core)
add_subdirectory(network)
add_subdirectory(protocol)

# Custom target to run all tests
add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --verbose
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all tests..."
)
