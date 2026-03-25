# Unit tests. Each test runs in its own folder with a copy of the config fixtures.
set(UNIT_TESTS
    acl_roles_handler
    akashi_utils
    aopacket
    area
    command_extension
    config_manager
    configstore
    dbmanager
    filesystem
    crypto
    jsonsettings
    mmdbreader
    packet
    packetcodec
    networkservice
    service
    music_manager
    playerstateobserver
    ulogger
)

foreach(name ${UNIT_TESTS})
    qt_add_executable(tst_${name} tst_${name}.cpp)
    target_link_libraries(tst_${name} PRIVATE akashi_core Qt6::Test)
    file(COPY fixtures/config DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/run_${name})
    add_test(NAME unit_${name} COMMAND tst_${name} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/run_${name})
    set_tests_properties(unit_${name} PROPERTIES TIMEOUT 60)
    if(WIN32)
        add_custom_command(TARGET tst_${name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:akashi_core> $<TARGET_FILE_DIR:tst_${name}>)
    endif()
endforeach()
