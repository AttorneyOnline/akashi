QT += network websockets core sql
QT -= gui

TEMPLATE = lib

# Apparently, Windows needs a static config to make a dynamic library?
# Look, I dunno.
# Linux works just fine with `shared` only.
CONFIG += shared static c++17

coverage {
    QMAKE_CXXFLAGS += --coverage -g -Og    # -fprofile-arcs -ftest-coverage
    LIBS += -lgcov
    CONFIG -= static
}

# Needed so that Windows doesn't do `release/` and `debug/` subfolders
# in the output directory.
CONFIG -= \
        copy_dir_files \
        debug_and_release \
        debug_and_release_target

DESTDIR = $$PWD/../bin

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# Enable this to print network messages to the console
DEFINES += NET_DEBUG

SOURCES += \
    src/acl_roles_handler.cpp \
    src/aoclient.cpp \
    src/network/aopacket.cpp \
    src/network/network_socket.cpp \
    src/area_data.cpp \
    src/command_extension.cpp \
    src/commands/area.cpp \
    src/commands/authentication.cpp \
    src/commands/casing.cpp \
    src/commands/command_helper.cpp \
    src/commands/messaging.cpp \
    src/commands/moderation.cpp \
    src/commands/music.cpp \
    src/commands/roleplay.cpp \
    src/config_manager.cpp \
    src/db_manager.cpp \
    src/discord.cpp \
    src/packets.cpp \
    src/server.cpp \
    src/testimony_recorder.cpp \
    src/advertiser.cpp \
    src/logger/u_logger.cpp \
    src/logger/writer_modcall.cpp \
    src/logger/writer_full.cpp \
    src/music_manager.cpp \
    src/packet/packet_factory.cpp \
    src/packet/packet_generic.cpp \
    src/packet/packet_hi.cpp \
    src/packet/packet_id.cpp \
    src/packet/packet_askchaa.cpp \
    src/packet/packet_casea.cpp \
    src/packet/packet_cc.cpp \
    src/packet/packet_ch.cpp \
    src/packet/packet_ct.cpp \
    src/packet/packet_de.cpp \
    src/packet/packet_ee.cpp \
    src/packet/packet_hp.cpp \
    src/packet/packet_mc.cpp \
    src/packet/packet_ms.cpp \
    src/packet/packet_pe.cpp \
    src/packet/packet_pw.cpp \
    src/packet/packet_rc.cpp \
    src/packet/packet_rd.cpp \
    src/packet/packet_rm.cpp \
    src/packet/packet_rt.cpp \
    src/packet/packet_setcase.cpp \
    src/packet/packet_zz.cpp

HEADERS += include/aoclient.h \
    include/acl_roles_handler.h \
    include/akashidefs.h \
    include/network/aopacket.h \
    include/network/network_socket.h \
    include/area_data.h \
    include/command_extension.h \
    include/config_manager.h \
    include/data_types.h \
    include/db_manager.h \
    include/discord.h \
    include/server.h \
    include/typedefs.h \
    include/advertiser.h \
    include/logger/u_logger.h \
    include/logger/writer_modcall.h \
    include/logger/writer_full.h \
    include/music_manager.h \
    include/packet/packet_factory.h \
    include/packet/packet_info.h \
    include/packet/packet_generic.h \
    include/packet/packet_hi.h \
    include/packet/packet_id.h \
    include/packet/packet_askchaa.h \
    include/packet/packet_casea.h \
    include/packet/packet_cc.h \
    include/packet/packet_ch.h \
    include/packet/packet_ct.h \
    include/packet/packet_de.h \
    include/packet/packet_ee.h \
    include/packet/packet_hp.h \
    include/packet/packet_mc.h \
    include/packet/packet_ms.h \
    include/packet/packet_pe.h \
    include/packet/packet_pw.h \
    include/packet/packet_rc.h \
    include/packet/packet_rd.h \
    include/packet/packet_rm.h \
    include/packet/packet_rt.h \
    include/packet/packet_setcase.h \
    include/packet/packet_zz.h
