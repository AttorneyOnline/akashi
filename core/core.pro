QT += network websockets core sql
QT -= gui

TEMPLATE = lib

# Apparently, Windows needs a static config to make a dynamic library?
# Look, I dunno.
# Linux works just fine with `shared` only.
CONFIG += shared static c++11

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
#DEFINES += NET_DEBUG

SOURCES += \
    src/aoclient.cpp \
    src/aopacket.cpp \
    src/area_data.cpp \
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
    src/ws_client.cpp \
    src/ws_proxy.cpp \
    src/advertiser.cpp \
    src/logger/u_logger.cpp \
    src/logger/writer_modcall.cpp \
    src/logger/writer_full.cpp \
    src/music_manager.cpp

HEADERS += include/aoclient.h \
    include/aopacket.h \
    include/area_data.h \
    include/config_manager.h \
    include/data_types.h \
    include/db_manager.h \
    include/discord.h \
    include/server.h \
    include/typedefs.h \
    include/ws_client.h \
    include/ws_proxy.h \
    include/advertiser.h \
    include/logger/u_logger.h \
    include/logger/writer_modcall.h \
    include/logger/writer_full.h \
    include/music_manager.h
