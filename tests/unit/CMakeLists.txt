# Unit tests. Each test runs in its own folder with a copy of the config fixtures.
set(UNIT_TESTS
    acl_roles_handler
    area
    config_loading
    configstore
    databaseservice
    dbmanager
    filesystem
    crypto
    jsonsettings
    mmdbreader
    packet
    packetcodec
    packetregistry
    handshake
    chat
    clientsession
    ic
    area_music
    moderation
    networkservice
    service
    jukebox
    playerdirectory
    testimony
    world
    arup_broadcaster
    playerstateobserver
    log_service
    command_registry
    event_bus
    text_filter
    auth_throttle
    plugin_interface
    plugin_manager
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

# MedievalParser lives in the medieval-speak plugin; compile its source directly for testing.
qt_add_executable(tst_medieval_parser tst_medieval_parser.cpp
    ${CMAKE_SOURCE_DIR}/plugins/medieval-speak/medieval_parser.cpp)
target_include_directories(tst_medieval_parser PRIVATE ${CMAKE_SOURCE_DIR}/plugins/medieval-speak)
target_link_libraries(tst_medieval_parser PRIVATE akashi_core Qt6::Test)
file(COPY fixtures/config DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/run_medieval_parser)
add_test(NAME unit_medieval_parser COMMAND tst_medieval_parser WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/run_medieval_parser)
set_tests_properties(unit_medieval_parser PROPERTIES TIMEOUT 60)
if(WIN32)
    add_custom_command(TARGET tst_medieval_parser POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:akashi_core> $<TARGET_FILE_DIR:tst_medieval_parser>)
endif()

# WriterSql lives in the sql-logger plugin; compile its source directly for testing.
qt_add_executable(tst_writer_sql tst_writer_sql.cpp
    ${CMAKE_SOURCE_DIR}/plugins/sql-logger/writer_sql.cpp)
target_include_directories(tst_writer_sql PRIVATE ${CMAKE_SOURCE_DIR}/plugins/sql-logger)
target_link_libraries(tst_writer_sql PRIVATE akashi_core Qt6::Test Qt6::Sql)
file(COPY fixtures/config DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/run_writer_sql)
add_test(NAME unit_writer_sql COMMAND tst_writer_sql WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/run_writer_sql)
set_tests_properties(unit_writer_sql PROPERTIES TIMEOUT 60)
if(WIN32)
    add_custom_command(TARGET tst_writer_sql POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:akashi_core> $<TARGET_FILE_DIR:tst_writer_sql>)
endif()
