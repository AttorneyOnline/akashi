cmake_minimum_required(VERSION 3.16)
project(core VERSION 1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)

find_package(QT NAMES Qt5 Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Network Sql WebSockets)

add_library(core
    include/acl_roles_handler.h
    include/advertiser.h
    include/akashidefs.h
    include/akashiutils.h
    include/aoclient.h
    include/area_data.h
    include/command_extension.h
    include/config_manager.h
    include/data_types.h
    include/db_manager.h
    include/discord.h
    include/logger/u_logger.h
    include/logger/writer_full.h
    include/logger/writer_modcall.h
    include/music_manager.h
    include/network/aopacket.h
    include/network/network_socket.h
    include/packet/packet_askchaa.h
    include/packet/packet_casea.h
    include/packet/packet_cc.h
    include/packet/packet_ch.h
    include/packet/packet_ct.h
    include/packet/packet_de.h
    include/packet/packet_ee.h
    include/packet/packet_factory.h
    include/packet/packet_generic.h
    include/packet/packet_hi.h
    include/packet/packet_hp.h
    include/packet/packet_id.h
    include/packet/packet_info.h
    include/packet/packet_mc.h
    include/packet/packet_ms.h
    include/packet/packet_pe.h
    include/packet/packet_pw.h
    include/packet/packet_rc.h
    include/packet/packet_rd.h
    include/packet/packet_rm.h
    include/packet/packet_rt.h
    include/packet/packet_setcase.h
    include/packet/packet_zz.h
    include/server.h
    include/typedefs.h
    src/acl_roles_handler.cpp
    src/advertiser.cpp
    src/aoclient.cpp
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
    src/logger/u_logger.cpp
    src/logger/writer_full.cpp
    src/logger/writer_modcall.cpp
    src/music_manager.cpp
    src/network/aopacket.cpp
    src/network/network_socket.cpp
    src/packet/packet_askchaa.cpp
    src/packet/packet_casea.cpp
    src/packet/packet_cc.cpp
    src/packet/packet_ch.cpp
    src/packet/packet_ct.cpp
    src/packet/packet_de.cpp
    src/packet/packet_ee.cpp
    src/packet/packet_factory.cpp
    src/packet/packet_generic.cpp
    src/packet/packet_hi.cpp
    src/packet/packet_hp.cpp
    src/packet/packet_id.cpp
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
    src/packets.cpp
    src/server.cpp
    src/testimony_recorder.cpp
)
target_compile_definitions(core PUBLIC
    NET_DEBUG
)

target_link_libraries(core PUBLIC
    Qt::Core
    Qt::Network
    Qt::Sql
    Qt::WebSockets
)

if(coverage)
    target_link_libraries(core PUBLIC
        gcov
    )

    target_compile_options(core
        --coverage
        -Og
        -g
    )
endif()

install(TARGETS core
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FRAMEWORK DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
