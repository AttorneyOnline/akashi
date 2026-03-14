# Unit tests. Each test runs in its own folder with a copy of the config fixtures.
set(UNIT_TESTS
    acl_roles_handler
    akashi_utils
    aopacket
    area
    command_extension
    config_manager
    crypto
    music_manager
)

foreach(name ${UNIT_TESTS})
    qt_add_executable(tst_${name} tst_${name}.cpp)
    target_link_libraries(tst_${name} PRIVATE akashi_testlib Qt6::Test)
    file(COPY fixtures/config DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/run_${name})
    add_test(NAME unit_${name} COMMAND tst_${name} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/run_${name})
    set_tests_properties(unit_${name} PROPERTIES TIMEOUT 60)
endforeach()
