cmake_minimum_required(VERSION 3.19)
project(akashi VERSION 1.9 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Network WebSockets Sql)
find_package(Qt6 REQUIRED COMPONENTS Core Network WebSockets Sql)

qt_standard_project_setup()
qt_add_executable(akashi
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
  src/main.cpp
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
)

target_link_libraries(akashi PRIVATE
    Qt6::Core
    Qt6::Sql
    Qt6::Network
    Qt6::WebSockets
)

target_include_directories(akashi PRIVATE src src/logger src/network src/packet)

set_target_properties(akashi PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

if(WIN32)
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set_property(TARGET akashi PROPERTY WIN32_EXECUTABLE true)
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/data/icon/akashi.rc")
    target_sources(akashi PRIVATE ${APP_ICON_RESOURCE_WINDOWS})
    set_target_properties(akashi PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:CONSOLE /ENTRY:mainCRTStartup")
  endif()
endif()

set_target_properties(akashi PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY $<1:${CMAKE_CURRENT_LIST_DIR}/bin>
        RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_CURRENT_LIST_DIR}/bin>)
