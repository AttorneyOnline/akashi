QT += network websockets core sql

TEMPLATE = lib

# shared static is required  by Windows.
CONFIG += shared static c++2a

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

DESTDIR = $$PWD/bin

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# Enable this to print network messages to the console
# DEFINES += NET_DEBUG

INCLUDEPATH += src

SOURCES += \
  src/acl_roles_handler.cpp \
  src/akashidefs.cpp \
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
  src/packet/packet_pr.cpp \
  src/packets.cpp \
  src/playerstateobserver.cpp \
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
  src/packet/packet_ma.cpp \
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

HEADERS += src/aoclient.h \
  src/acl_roles_handler.h \
  src/akashidefs.h \
  src/akashiutils.h \
  src/network/aopacket.h \
  src/network/network_socket.h \
  src/area_data.h \
  src/command_extension.h \
  src/config_manager.h \
  src/data_types.h \
  src/db_manager.h \
  src/discord.h \
  src/packet/packet_pr.h \
  src/playerstateobserver.h \
  src/server.h \
  src/typedefs.h \
  src/advertiser.h \
  src/logger/u_logger.h \
  src/logger/writer_modcall.h \
  src/logger/writer_full.h \
  src/music_manager.h \
  src/packet/packet_factory.h \
  src/packet/packet_info.h \
  src/packet/packet_generic.h \
  src/packet/packet_hi.h \
  src/packet/packet_id.h \
  src/packet/packet_askchaa.h \
  src/packet/packet_casea.h \
  src/packet/packet_cc.h \
  src/packet/packet_ch.h \
  src/packet/packet_ct.h \
  src/packet/packet_de.h \
  src/packet/packet_ee.h \
  src/packet/packet_hp.h \
  src/packet/packet_ma.h \
  src/packet/packet_mc.h \
  src/packet/packet_ms.h \
  src/packet/packet_pe.h \
  src/packet/packet_pw.h \
  src/packet/packet_rc.h \
  src/packet/packet_rd.h \
  src/packet/packet_rm.h \
  src/packet/packet_rt.h \
  src/packet/packet_setcase.h \
  src/packet/packet_zz.h
