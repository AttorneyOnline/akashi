QT += network websockets core sql
QT -= gui

TEMPLATE = lib

CONFIG += shared c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# Enable this to print network messages tothe console
#DEFINES += NET_DEBUG

SOURCES += \
    src/advertiser.cpp \
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
    src/logger.cpp \
    src/packets.cpp \
    src/server.cpp \
    src/testimony_recorder.cpp \
    src/ws_client.cpp \
    src/ws_proxy.cpp

HEADERS += include/advertiser.h \
    include/aoclient.h \
    include/aopacket.h \
    include/area_data.h \
    include/config_manager.h \
    include/db_manager.h \
    include/logger.h \
    include/server.h \
    include/ws_client.h \
    include/ws_proxy.h
