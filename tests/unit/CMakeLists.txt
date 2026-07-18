# Unit tests. Each test runs in its own folder with a copy of the config fixtures.
set(UNIT_TESTS
    acl_roles_handler
    area
    config_loading
    configstore
    databaseservice
    dbmanager
    filesystem
    floodguard
    crypto
    jsonsettings
    mmdbreader
    packet
    packet_interceptors
    packetcodec
    packetregistry
    handshake
    chat
    clientsession
    session_stage
    ic
    area_music
    moderation
    networkservice
    websocket_transport
    service
    server_lifecycle
    jukebox
    playerdirectory
    testimony
    world
    arup_broadcaster
    playerstateobserver
    log_service
    writer_text
    moderation_service
    medieval_parser
    text_utils
    command_registry
    console_menu
    event_registry
    scheduler
    text_filter
    auth_throttle
    authenticator_registry
    username_authenticator
    plugin_interface
    plugin_manager
)

# The webhook tests drive real HTTP against the webhook receiver test tool.
if(TARGET Qt6::HttpServer)
    list(APPEND UNIT_TESTS webhook_receiver discord_hook server_publisher)
else()
    message(STATUS "Qt6 HttpServer not found - skipping the webhook tests")
endif()

foreach(name ${UNIT_TESTS})
    qt_add_executable(tst_${name} tst_${name}.cpp)
    target_link_libraries(tst_${name} PRIVATE akashi_core akashi::testtools Qt6::Test)
    # Unit tests are white-box: they may use the core's private headers.
    target_include_directories(tst_${name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    file(COPY fixtures/config DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/run_${name})
    add_test(NAME unit_${name} COMMAND tst_${name} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/run_${name})
    set_tests_properties(unit_${name} PROPERTIES TIMEOUT 60)
    if(WIN32)
        add_custom_command(TARGET tst_${name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:akashi_core> $<TARGET_FILE_DIR:tst_${name}>)
    endif()
endforeach()


# WriterSql lives in the sql-logger plugin; compile its source directly for testing.
qt_add_executable(tst_writer_sql tst_writer_sql.cpp
    ${CMAKE_SOURCE_DIR}/plugins/sql-logger/writer_sql.cpp)
target_include_directories(tst_writer_sql PRIVATE ${CMAKE_SOURCE_DIR}/plugins/sql-logger ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_writer_sql PRIVATE akashi_core Qt6::Test Qt6::Sql)
file(COPY fixtures/config DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/run_writer_sql)
add_test(NAME unit_writer_sql COMMAND tst_writer_sql WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/run_writer_sql)
set_tests_properties(unit_writer_sql PROPERTIES TIMEOUT 60)
if(WIN32)
    add_custom_command(TARGET tst_writer_sql POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:akashi_core> $<TARGET_FILE_DIR:tst_writer_sql>)
endif()

# The modbot's rule engine and queue live in the plugin; compile them directly.
qt_add_executable(tst_modbot_rules tst_modbot_rules.cpp
    ${CMAKE_SOURCE_DIR}/plugins/modbot/modbot_rules.cpp
    ${CMAKE_SOURCE_DIR}/plugins/modbot/modbot_worker.cpp)
target_include_directories(tst_modbot_rules PRIVATE ${CMAKE_SOURCE_DIR}/plugins/modbot ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_modbot_rules PRIVATE akashi_core Qt6::Test Qt6::Sql)
file(COPY fixtures/config DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/run_modbot_rules)
add_test(NAME unit_modbot_rules COMMAND tst_modbot_rules WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/run_modbot_rules)
set_tests_properties(unit_modbot_rules PROPERTIES TIMEOUT 60)
if(WIN32)
    add_custom_command(TARGET tst_modbot_rules POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:akashi_core> $<TARGET_FILE_DIR:tst_modbot_rules>)
endif()
