cmake_minimum_required(VERSION 3.14)

project(core LANGUAGES CXX)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core WebSockets Network Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core WebSockets Network Sql)

add_library(core
    src/acl_roles_handler.cpp
    src/aoclient.cpp
    src/network/aopacket.cpp
    src/network/network_socket.cpp
    src/area_data.cpp
    src/command_extension.cpp
    src/commands/area.cpp
    src/commands/authentication.cpp
    src/commands/casing.cpp
    src/commands/command_helper.cpp
    src/commands/messaging.cpp
    src/commands/moderation.cpp
    src/commands/music.cpp
    src/commands/roleplay.cpp
    src/config_manager.cpp
    src/db_manager.cpp
    src/discord.cpp
    src/packets.cpp
    src/server.cpp
    src/testimony_recorder.cpp
    src/advertiser.cpp
    src/logger/u_logger.cpp
    src/logger/writer_modcall.cpp
    src/logger/writer_full.cpp
    src/music_manager.cpp
    src/packet/packet_factory.cpp
    src/packet/packet_generic.cpp
    src/packet/packet_hi.cpp
    src/packet/packet_id.cpp
    src/packet/packet_askchaa.cpp
    src/packet/packet_casea.cpp
    src/packet/packet_cc.cpp
    src/packet/packet_ch.cpp
    src/packet/packet_ct.cpp
    src/packet/packet_de.cpp
    src/packet/packet_ee.cpp
    src/packet/packet_hp.cpp
    src/packet/packet_mc.cpp
    src/packet/packet_ms.cpp
    src/packet/packet_pe.cpp
    src/packet/packet_pw.cpp
    src/packet/packet_rc.cpp
    src/packet/packet_rd.cpp
    src/packet/packet_rm.cpp
    src/packet/packet_rt.cpp
    src/packet/packet_setcase.cpp
    src/packet/packet_zz.cpp

    src/aoclient.h
        src/acl_roles_handler.h
        src/akashidefs.h
        src/akashiutils.h
        src/network/aopacket.h
        src/network/network_socket.h
        src/area_data.h
        src/command_extension.h
        src/config_manager.h
        src/data_types.h
        src/db_manager.h
        src/discord.h
        src/server.h
        src/typedefs.h
        src/advertiser.h
        src/logger/u_logger.h
        src/logger/writer_modcall.h
        src/logger/writer_full.h
        src/music_manager.h
        src/packet/packet_factory.h
        src/packet/packet_info.h
        src/packet/packet_generic.h
        src/packet/packet_hi.h
        src/packet/packet_id.h
        src/packet/packet_askchaa.h
        src/packet/packet_casea.h
        src/packet/packet_cc.h
        src/packet/packet_ch.h
        src/packet/packet_ct.h
        src/packet/packet_de.h
        src/packet/packet_ee.h
        src/packet/packet_hp.h
        src/packet/packet_mc.h
        src/packet/packet_ms.h
        src/packet/packet_pe.h
        src/packet/packet_pw.h
        src/packet/packet_rc.h
        src/packet/packet_rd.h
        src/packet/packet_rm.h
        src/packet/packet_rt.h
        src/packet/packet_setcase.h
        src/packet/packet_zz.h
)

target_include_directories(core
    PUBLIC src/
    PUBLIC src/network
    PUBLIC src/commands
    PUBLIC src/logger
    PUBLIC src/packet
)

target_link_libraries(core PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::WebSockets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Sql
)

target_compile_definitions(core PRIVATE
    AKASHICORE_LIBRARY TRUE
)

set_target_properties(core PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY $<1:${CMAKE_SOURCE_DIR}/bin>
    RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_SOURCE_DIR}/bin>
)
