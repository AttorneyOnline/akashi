cmake_minimum_required(VERSION 3.19)
project(akashi VERSION 1.9 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Network WebSockets Sql Test)

enable_testing()

qt_standard_project_setup()

set(AKASHI_SOURCES
  src/commands/area.cpp
  src/commands/authentication.cpp
  src/commands/casing.cpp
  src/commands/command_helper.cpp
  src/commands/messaging.cpp
  src/commands/moderation.cpp
  src/commands/music.cpp
  src/commands/roleplay.cpp
  src/logger/u_logger.cpp
  src/logger/u_logger.h
  src/logger/writer_full.cpp
  src/logger/writer_full.h
  src/logger/writer_modcall.cpp
  src/logger/writer_modcall.h
  src/network/aopacket.cpp
  src/network/aopacket.h
  src/network/network_socket.cpp
  src/network/network_socket.h
  src/packet/packet_askchaa.cpp
  src/packet/packet_askchaa.h
  src/packet/packet_casea.cpp
  src/packet/packet_casea.h
  src/packet/packet_cc.cpp
  src/packet/packet_cc.h
  src/packet/packet_ch.cpp
  src/packet/packet_ch.h
  src/packet/packet_ct.cpp
  src/packet/packet_ct.h
  src/packet/packet_de.cpp
  src/packet/packet_de.h
  src/packet/packet_ee.cpp
  src/packet/packet_ee.h
  src/packet/packet_factory.cpp
  src/packet/packet_factory.h
  src/packet/packet_generic.cpp
  src/packet/packet_generic.h
  src/packet/packet_hi.cpp
  src/packet/packet_hi.h
  src/packet/packet_hp.cpp
  src/packet/packet_hp.h
  src/packet/packet_id.cpp
  src/packet/packet_id.h
  src/packet/packet_info.h
  src/packet/packet_ma.cpp
  src/packet/packet_ma.h
  src/packet/packet_mc.cpp
  src/packet/packet_mc.h
  src/packet/packet_ms.cpp
  src/packet/packet_ms.h
  src/packet/packet_pe.cpp
  src/packet/packet_pe.h
  src/packet/packet_pr.cpp
  src/packet/packet_pr.h
  src/packet/packet_pw.cpp
  src/packet/packet_pw.h
  src/packet/packet_rc.cpp
  src/packet/packet_rc.h
  src/packet/packet_rd.cpp
  src/packet/packet_rd.h
  src/packet/packet_rm.cpp
  src/packet/packet_rm.h
  src/packet/packet_rt.cpp
  src/packet/packet_rt.h
  src/packet/packet_setcase.cpp
  src/packet/packet_setcase.h
  src/packet/packet_zz.cpp
  src/packet/packet_zz.h
  src/acl_roles_handler.cpp
  src/acl_roles_handler.h
  src/akashiutils.h
  src/aoclient.cpp
  src/aoclient.h
  src/area_data.cpp
  src/area_data.h
  src/command_extension.cpp
  src/command_extension.h
  src/config_manager.cpp
  src/config_manager.h
  src/crypto_helper.h
  src/data_types.h
  src/db_manager.cpp
  src/db_manager.h
  src/discord.cpp
  src/discord.h
  src/medieval_parser.cpp
  src/medieval_parser.h
  src/music_manager.cpp
  src/music_manager.h
  src/packets.cpp
  src/playerstateobserver.cpp
  src/playerstateobserver.h
  src/server.cpp
  src/server.h
  src/serverpublisher.cpp
  src/serverpublisher.h
  src/testimony_recorder.cpp
  src/typedefs.h
  src2/core/config_entry.cpp
  src2/core/config_store.cpp
  src2/core/database_service.cpp
  src2/core/exit_code.h
  src2/core/service.cpp
  src2/core/service_registry.cpp
  src2/core/filesystem_service.cpp
  src2/core/json_settings.cpp
  src2/core/json_settings.h
  src2/core/mmdb_reader.cpp
  src2/core/mmdb_reader.h
  src2/core/server_settings.h
  src2/core/settings.cpp
  src2/core/server_context.cpp
  src2/core/server_context.h
  src2/include/akashi/config_entry.h
  src2/include/akashi/config_store.h
  src2/include/akashi/database_service.h
  src2/include/akashi/filesystem_service.h
  src2/include/akashi/service.h
  src2/include/akashi/service_registry.h
  src2/include/akashi/settings.h
  src2/proto/ao2_protocol.h
  src2/proto/client_profile.h
  src2/proto/packet_codec.cpp
  src2/proto/packet_codec.h
  src2/proto/packet.cpp
  src2/proto/packet.h
)

add_library(akashi_core SHARED ${AKASHI_SOURCES})
include(GenerateExportHeader)
generate_export_header(akashi_core BASE_NAME AKASHI_CORE)
target_include_directories(akashi_core PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/logger>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/network>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/packet>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src2>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src2/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
)

# Header-only library for the plugin SDK, filled in during M9 and M10.
add_library(akashi_pluginapi INTERFACE)
target_include_directories(akashi_pluginapi INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src2/include>"
    "$<INSTALL_INTERFACE:include>"
)
target_link_libraries(akashi_core PUBLIC
    Qt6::Core
    Qt6::Sql
    Qt6::Network
    Qt6::WebSockets
)

qt_add_executable(akashi src/main.cpp)
target_link_libraries(akashi PRIVATE akashi_core)

set_target_properties(akashi PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  if(WIN32)
    set_property(TARGET akashi PROPERTY WIN32_EXECUTABLE true)
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/data/icon/akashi.rc")
    target_sources(akashi PRIVATE ${APP_ICON_RESOURCE_WINDOWS})
    set_target_properties(akashi PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:CONSOLE /ENTRY:mainCRTStartup")
  endif()
  target_compile_definitions(akashi_core PRIVATE QT_NO_DEBUG_OUTPUT)
  target_compile_definitions(akashi PRIVATE QT_NO_DEBUG_OUTPUT)
endif()

# The $<1:...> wrapper keeps multi-config generators from adding per-config subfolders.
set_target_properties(akashi PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY $<1:${CMAKE_CURRENT_LIST_DIR}/bin>
        RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_CURRENT_LIST_DIR}/bin>)

# The server loads the core library from its own folder.
set_target_properties(akashi_core PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY $<1:${CMAKE_CURRENT_LIST_DIR}/bin>
        RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_CURRENT_LIST_DIR}/bin>)

# Install rules, so plugins can later use find_package(akashi).
include(CMakePackageConfigHelpers)
install(TARGETS akashi_core akashi_pluginapi EXPORT akashiTargets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)
install(DIRECTORY src2/include/ DESTINATION include)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/akashi_core_export.h DESTINATION include)
install(EXPORT akashiTargets NAMESPACE akashi:: DESTINATION lib/cmake/akashi)
configure_package_config_file(cmake/akashiConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/akashiConfig.cmake
    INSTALL_DESTINATION lib/cmake/akashi)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/akashiConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/akashiConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/akashiConfigVersion.cmake
    DESTINATION lib/cmake/akashi)

add_subdirectory(tests)
